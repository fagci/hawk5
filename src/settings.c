#include "settings.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include <string.h>

uint8_t BL_TIME_VALUES[7] = {0, 5, 10, 20, 60, 120, 255};
const char *BL_TIME_NAMES[7] = {"Off",  "5s",   "10s", "20s",
                                "1min", "2min", "On"};

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
    "5s", "10s",   "30s",   "1min",  "2min",  "5min",  "None",
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
    .mainApp = 13,
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
