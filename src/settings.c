#include "settings.h"
#include "apps/apps.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "helper/measurements.h"
#include "misc.h"
#include "radio.h"
#include "scheduler.h"
#include <string.h>

static uint32_t saveTime;

static const uint16_t BAT_CAL_MIN = 1900;

static const char *YES_NO[] = {"No", "Yes"};
static const char *ON_OFF[] = {"Off", "On"};

uint8_t BL_TIME_VALUES[7] = {0, 5, 10, 20, 60, 120, 255};

const char *BL_SQL_MODE_NAMES[3] = {"Off", "On", "Open"};
const char *CH_DISPLAY_MODE_NAMES[3] = {"Name+F", "F", "Name"};
const char *rogerNames[2] = {"None", "Tiny"};
const char *FC_TIME_NAMES[4] = {"0.2s", "0.4s", "0.8s", "1.6s"};
const char *MW_NAMES[4] = {
    [MW_OFF] = "Off",
    [MW_ON] = "On",
    [MW_SWITCH] = "Switch",
    [MW_EXTRA] = "Extra",
};
const char *EEPROM_TYPE_NAMES[6] = {
    "BL24C64 #", // 010
    "BL24C128",  // 011
    "BL24C256",  // 100
    "BL24C512",  // 101
    "BL24C1024", // 110
    "M24M02",    // 111
};
uint32_t SCAN_TIMEOUTS[15] = {
    0,         100,       200,           300,           400,
    500,       1000 * 1,  1000 * 3,      1000 * 5,      1000 * 10,
    1000 * 30, 1000 * 60, 1000 * 60 * 2, 1000 * 60 * 5, UINT32_MAX,
};

char *SCAN_TIMEOUT_NAMES[15] = {
    "0",  "100ms", "200ms", "300ms", "400ms", "500ms", "1s",   "3s",
    "5s", "10s",   "30s",   "1m",    "2m",    "5m",    "None",
};

static const uint8_t PATCH3_PREAMBLE[] = {0x15, 0x00, 0x03, 0x74,
                                          0x0b, 0xd4, 0x84, 0x60};

Settings gSettings = (Settings){
    .eepromType = EEPROM_UNKNOWN,
    .batsave = 4,
    .vox = 0,
    .backlight = 3,
    .txTime = 0,
    .currentScanlist = SCANLIST_ALL,
    .roger = 0,
    .scanmode = 0,
    .chDisplayMode = 0,
    .beep = false,
    .keylock = false,
    .busyChannelTxLock = false,
    .ste = true,
    .repeaterSte = true,
    .dtmfdecode = false,
    .brightness = 8,
    .contrast = 8,
    .mainApp = APP_VFO1,
    .sqOpenedTimeout = SCAN_TO_NONE,
    .sqClosedTimeout = SCAN_TO_2s,
    .sqlOpenTime = 1,
    .sqlCloseTime = 1,
    .skipGarbageFrequencies = true,
    .activeVFO = 0,
    .backlightOnSquelch = BL_SQL_ON,
    .batteryCalibration = 2000,
    .batteryType = BAT_1600,
    .batteryStyle = BAT_PERCENT,
    .upconverter = 0,
    .deviation = 130, // 1300
};

const uint32_t EEPROM_SIZES[6] = {
    8192, 16384, 32768, 65536, 131072, 262144,
};

const uint16_t PAGE_SIZES[6] = {
    32, 64, 64, 128, 128, 256,
};

void SETTINGS_Save(void) {
  EEPROM_WriteBuffer(SETTINGS_OFFSET, &gSettings, SETTINGS_SIZE);
}

void SETTINGS_Load(void) {
  EEPROM_ReadBuffer(SETTINGS_OFFSET, &gSettings, SETTINGS_SIZE);
}

void SETTINGS_DelayedSave(void) { SETTINGS_Save(); }

uint32_t SETTINGS_GetFilterBound(void) {
  return gSettings.bound_240_280 ? VHF_UHF_BOUND2 : VHF_UHF_BOUND1;
}

uint32_t SETTINGS_GetEEPROMSize(void) {
  return EEPROM_SIZES[gSettings.eepromType];
}

uint16_t SETTINGS_GetPageSize(void) { return PAGE_SIZES[gSettings.eepromType]; }

bool SETTINGS_IsPatchPresent() {
  if (SETTINGS_GetEEPROMSize() < 32768) {
    return false;
  }
  uint8_t buf[8];
  EEPROM_ReadBuffer(SETTINGS_GetEEPROMSize() - PATCH_SIZE, buf, 8);
  return memcmp(buf, PATCH3_PREAMBLE, 8) == 0;
}

bool dirty[SETTING_COUNT];

uint32_t SETTINGS_GetValue(Setting s) {
  switch (s) {
  case SETTING_FCTIME:
    return gSettings.fcTime;
  case SETTING_EEPROMTYPE:
    return gSettings.eepromType;
  case SETTING_BATSAVE:
    return gSettings.batsave;
  case SETTING_VOX:
    return gSettings.vox;
  case SETTING_BACKLIGHT:
    return gSettings.backlight;
  case SETTING_TXTIME:
    return gSettings.txTime;
  case SETTING_CURRENTSCANLIST:
    return gSettings.currentScanlist;
  case SETTING_ROGER:
    return gSettings.roger;
  case SETTING_SCANMODE:
    return gSettings.scanmode;
  case SETTING_CHDISPLAYMODE:
    return gSettings.chDisplayMode;
  case SETTING_BEEP:
    return gSettings.beep;
  case SETTING_PTT_LOCK:
    return gSettings.pttLock;
  case SETTING_KEYLOCK:
    return gSettings.keylock;
  case SETTING_MULTIWATCH:
    return gSettings.mWatch;
  case SETTING_BUSYCHANNELTXLOCK:
    return gSettings.busyChannelTxLock;
  case SETTING_STE:
    return gSettings.ste;
  case SETTING_REPEATERSTE:
    return gSettings.repeaterSte;
  case SETTING_DTMFDECODE:
    return gSettings.dtmfdecode;
  case SETTING_BRIGHTNESS_H:
    return gSettings.brightness;
  case SETTING_BRIGHTNESS_L:
    return gSettings.brightnessLow;
  case SETTING_CONTRAST:
    return gSettings.contrast;
  case SETTING_MAINAPP:
    return gSettings.mainApp;
  case SETTING_SQOPENEDTIMEOUT:
    return gSettings.sqOpenedTimeout;
  case SETTING_SQCLOSEDTIMEOUT:
    return gSettings.sqClosedTimeout;
  case SETTING_SQLOPENTIME:
    return gSettings.sqlOpenTime;
  case SETTING_SQLCLOSETIME:
    return gSettings.sqlCloseTime;
  case SETTING_SKIPGARBAGEFREQUENCIES:
    return gSettings.skipGarbageFrequencies;
  case SETTING_ACTIVEVFO:
    return gSettings.activeVFO;
  case SETTING_BACKLIGHTONSQUELCH:
    return gSettings.backlightOnSquelch;
  case SETTING_BATTERYCALIBRATION:
    return gSettings.batteryCalibration;
  case SETTING_BATTERYTYPE:
    return gSettings.batteryType;
  case SETTING_BATTERYSTYLE:
    return gSettings.batteryStyle;
  case SETTING_UPCONVERTER:
    return gSettings.upconverter;
  case SETTING_DEVIATION:
    return gSettings.deviation;
  case SETTING_MIC:
    return gSettings.mic;
  case SETTING_COUNT:
    return SETTING_COUNT;
  case SETTING_SHOWLEVELINVFO:
    return gSettings.showLevelInVFO;
  case SETTING_BOUND240_280:
    return gSettings.bound_240_280;
  case SETTING_NOLISTEN:
    return gSettings.noListen;
  case SETTING_SI4732POWEROFF:
    return gSettings.si4732PowerOff;
  case SETTING_TONELOCAL:
    return gSettings.toneLocal;
  }
  return 0;
}

void SETTINGS_SetValue(Setting s, uint32_t v) {
  uint32_t ov = SETTINGS_GetValue(s);

  switch (s) {
  case SETTING_FCTIME:
    gSettings.fcTime = v;
    break;
  case SETTING_EEPROMTYPE:
    gSettings.eepromType = v;
    break;
  case SETTING_BATSAVE:
    gSettings.batsave = v;
    break;
  case SETTING_VOX:
    gSettings.vox = v;
    break;
  case SETTING_BACKLIGHT:
    gSettings.backlight = v;
    BACKLIGHT_Init();
    break;
  case SETTING_TXTIME:
    gSettings.txTime = v;
    break;
  case SETTING_CURRENTSCANLIST:
    gSettings.currentScanlist = v;
    break;
  case SETTING_ROGER:
    gSettings.roger = v;
    break;
  case SETTING_SCANMODE:
    gSettings.scanmode = v;
    break;
  case SETTING_CHDISPLAYMODE:
    gSettings.chDisplayMode = v;
    break;
  case SETTING_BEEP:
    gSettings.beep = v;
    break;
  case SETTING_KEYLOCK:
    gSettings.keylock = v;
    break;
  case SETTING_MULTIWATCH:
    gSettings.mWatch = v;
    RADIO_ToggleMultiwatch(&gRadioState, gSettings.mWatch);
    break;
  case SETTING_PTT_LOCK:
    gSettings.pttLock = v;
    break;
  case SETTING_BUSYCHANNELTXLOCK:
    gSettings.busyChannelTxLock = v;
    break;
  case SETTING_STE:
    gSettings.ste = v;
    break;
  case SETTING_REPEATERSTE:
    gSettings.repeaterSte = v;
    break;
  case SETTING_DTMFDECODE:
    gSettings.dtmfdecode = v;
    break;
  case SETTING_BRIGHTNESS_H:
    gSettings.brightness = v;
    BACKLIGHT_Init();
    break;
  case SETTING_BRIGHTNESS_L:
    gSettings.brightnessLow = v;
    break;
  case SETTING_CONTRAST:
    gSettings.contrast = v;
    ST7565_Init(false);
    break;
  case SETTING_MAINAPP:
    gSettings.mainApp = v;
    break;
  case SETTING_SQOPENEDTIMEOUT:
    gSettings.sqOpenedTimeout = v;
    break;
  case SETTING_SQCLOSEDTIMEOUT:
    gSettings.sqClosedTimeout = v;
    break;
  case SETTING_SQLOPENTIME:
    gSettings.sqlOpenTime = v;
    break;
  case SETTING_SQLCLOSETIME:
    gSettings.sqlCloseTime = v;
    break;
  case SETTING_SKIPGARBAGEFREQUENCIES:
    gSettings.skipGarbageFrequencies = v;
    break;
  case SETTING_ACTIVEVFO:
    gSettings.activeVFO = v;
    break;
  case SETTING_BACKLIGHTONSQUELCH:
    gSettings.backlightOnSquelch = v;
    break;
  case SETTING_BATTERYCALIBRATION:
    gSettings.batteryCalibration = v;
    break;
  case SETTING_BATTERYTYPE:
    gSettings.batteryType = v;
    break;
  case SETTING_BATTERYSTYLE:
    gSettings.batteryStyle = v;
    break;
  case SETTING_UPCONVERTER:
    gSettings.upconverter = v;
    break;
  case SETTING_DEVIATION:
    gSettings.deviation = v;
    break;
  case SETTING_MIC:
    gSettings.mic = v;
    break;
  case SETTING_SHOWLEVELINVFO:
    gSettings.showLevelInVFO = v;
    break;
  case SETTING_BOUND240_280:
    gSettings.bound_240_280 = v;
    ctx->dirty[PARAM_FILTER] = true; // filter update
    RADIO_ApplySettings(ctx);
    break;
  case SETTING_NOLISTEN:
    gSettings.noListen = v;
    break;
  case SETTING_SI4732POWEROFF:
    gSettings.si4732PowerOff = v;
    break;
  case SETTING_TONELOCAL:
    gSettings.toneLocal = v;
    break;
  case SETTING_COUNT:
    return;
  }

  if (v != ov) {
    dirty[s] = true;
    saveTime = Now() + 1000;
  }
}

const char *SETTINGS_GetValueString(Setting s) {
  static char buf[16] = "unk";
  uint32_t v = SETTINGS_GetValue(s);

  switch (s) {
  case SETTING_SHOWLEVELINVFO:
  case SETTING_NOLISTEN:
  case SETTING_SI4732POWEROFF:
  case SETTING_TONELOCAL:
  case SETTING_SKIPGARBAGEFREQUENCIES:
  case SETTING_DTMFDECODE:
  case SETTING_PTT_LOCK:
    return YES_NO[v];

  case SETTING_BEEP:
  case SETTING_REPEATERSTE:
  case SETTING_KEYLOCK:
  case SETTING_STE:
    return ON_OFF[v];
  case SETTING_MULTIWATCH:
    return MW_NAMES[v];

  case SETTING_EEPROMTYPE:
    return EEPROM_TYPE_NAMES[v];
  case SETTING_BOUND240_280:
    return FLT_BOUND_NAMES[v];
  case SETTING_ROGER:
    return rogerNames[v];
  case SETTING_CHDISPLAYMODE:
    return CH_DISPLAY_MODE_NAMES[v];
  case SETTING_MAINAPP:
    return apps[v].name;
  case SETTING_SQOPENEDTIMEOUT:
  case SETTING_SQCLOSEDTIMEOUT:
    return SCAN_TIMEOUT_NAMES[v];
  case SETTING_BACKLIGHTONSQUELCH:
    return BL_SQL_MODE_NAMES[v];
  case SETTING_BATTERYTYPE:
    return BATTERY_TYPE_NAMES[v];
  case SETTING_BATTERYSTYLE:
    return BATTERY_STYLE_NAMES[v];
  case SETTING_FCTIME:
    return FC_TIME_NAMES[v];
  case SETTING_BACKLIGHT:
    if (BL_TIME_VALUES[v] == 0) {
      return ON_OFF[0];
    } else if (BL_TIME_VALUES[v] == 255) {
      return ON_OFF[1];
    }
    sprintf(buf, "%us", BL_TIME_VALUES[v]);
    break;

  case SETTING_BATTERYCALIBRATION: {
    uint32_t vol = BATTERY_GetPreciseVoltage(v);
    sprintf(buf, "%u.%04u (%u)", vol / 10000, vol % 10000, v);
  } break;
  case SETTING_UPCONVERTER:
    mhzToS(buf, v);
    break;

  case SETTING_SQLOPENTIME:
  case SETTING_SQLCLOSETIME:
    snprintf(buf, 15, "%ums", v * 5);
    break;
  case SETTING_DEVIATION:
    snprintf(buf, 15, "%u", v * 10);
    break;
  case SETTING_CONTRAST:
    sprintf(buf, "%d", v - 8);
    break;

  case SETTING_MIC:
  case SETTING_COUNT:
  case SETTING_ACTIVEVFO:
  case SETTING_BRIGHTNESS_L:
  case SETTING_BRIGHTNESS_H:
    snprintf(buf, 15, "%u", v);
    break;

  case SETTING_CURRENTSCANLIST:
    ScanlistStr(v, buf);
    break;

  case SETTING_BATSAVE:
  case SETTING_VOX:
  case SETTING_TXTIME:
  case SETTING_SCANMODE:
  case SETTING_BUSYCHANNELTXLOCK:
    return "N/a";
  }
  return buf;
}

void SETTINGS_IncDecValue(Setting s, bool inc) {
  uint32_t mi = 0;
  uint32_t ma = UINT32_MAX;
  uint32_t v = SETTINGS_GetValue(s);
  switch (s) {
  case SETTING_SHOWLEVELINVFO:
  case SETTING_NOLISTEN:
  case SETTING_SI4732POWEROFF:
  case SETTING_TONELOCAL:
  case SETTING_SKIPGARBAGEFREQUENCIES:
  case SETTING_DTMFDECODE:
  case SETTING_BEEP:
  case SETTING_REPEATERSTE:
  case SETTING_KEYLOCK:
  case SETTING_PTT_LOCK:
  case SETTING_STE:
    ma = 2;
    break;
  case SETTING_MULTIWATCH:
    ma = ARRAY_SIZE(MW_NAMES);
    break;

  case SETTING_EEPROMTYPE:
    ma = ARRAY_SIZE(EEPROM_TYPE_NAMES);
    break;
  case SETTING_BOUND240_280:
    ma = ARRAY_SIZE(FLT_BOUND_NAMES);
    break;
  case SETTING_ROGER:
    ma = ARRAY_SIZE(rogerNames);
    break;
  case SETTING_CHDISPLAYMODE:
    ma = ARRAY_SIZE(CH_DISPLAY_MODE_NAMES);
    break;
  case SETTING_MAINAPP: {
    int8_t found_index = -1;
    for (uint8_t i = 0; i < RUN_APPS_COUNT; i++) {
      if (appsAvailableToRun[i] == v) {
        found_index = i;
        break;
      }
    }

    if (found_index == -1) {
      v = appsAvailableToRun[0];
    }

    int8_t next_index = IncDecI(found_index, 0, RUN_APPS_COUNT, inc);
    v = appsAvailableToRun[next_index];
    SETTINGS_SetValue(s, v);
    return;
  }
  case SETTING_SQOPENEDTIMEOUT:
  case SETTING_SQCLOSEDTIMEOUT:
    ma = ARRAY_SIZE(SCAN_TIMEOUT_NAMES);
    break;
  case SETTING_BACKLIGHTONSQUELCH:
    ma = ARRAY_SIZE(BL_SQL_MODE_NAMES);
    break;
  case SETTING_BATTERYTYPE:
    ma = ARRAY_SIZE(BATTERY_TYPE_NAMES);
    break;
  case SETTING_BATTERYSTYLE:
    ma = ARRAY_SIZE(BATTERY_STYLE_NAMES);
    break;
  case SETTING_FCTIME:
    ma = ARRAY_SIZE(FC_TIME_NAMES);
    break;

  case SETTING_BATTERYCALIBRATION:
    break;
  case SETTING_UPCONVERTER:
    break;
  case SETTING_BACKLIGHT:
    break;
  case SETTING_CURRENTSCANLIST:
    break;
  case SETTING_BRIGHTNESS_L:
    break;
  case SETTING_BRIGHTNESS_H:
    break;
  case SETTING_CONTRAST:
    break;

  case SETTING_SQLOPENTIME:
  case SETTING_SQLCLOSETIME:
    // TODO: see datasheet
    break;
  case SETTING_DEVIATION:
    ma = 256;
    break;
  case SETTING_MIC:
    ma = 16;
    break;

  case SETTING_COUNT:
    ma = SETTING_COUNT;
    break;
  case SETTING_ACTIVEVFO:
    // TODO: radio get vfo count
    break;

  case SETTING_BATSAVE:
  case SETTING_VOX:
  case SETTING_TXTIME:
  case SETTING_SCANMODE:
  case SETTING_BUSYCHANNELTXLOCK:
    break;
  }

  SETTINGS_SetValue(s, IncDecI(v, mi, ma, inc));
}

void SETTINGS_UpdateSave() {
  if (saveTime && Now() > saveTime) {
    saveTime = 0;
    SETTINGS_Save();
    for (uint8_t i = 0; i < SETTING_COUNT; ++i) {
      dirty[i] = false;
    }
  }
}
