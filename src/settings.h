#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define getsize(V) char (*__ #V)(void)[sizeof(V)] = 1;

#define SCANLIST_ALL 0

typedef enum {
  SETTING_EEPROMTYPE,
  SETTING_BATSAVE,
  SETTING_VOX,
  SETTING_BACKLIGHT,
  SETTING_TXTIME,
  SETTING_CURRENTSCANLIST,
  SETTING_ROGER,
  SETTING_SCANMODE,
  SETTING_CHDISPLAYMODE,
  SETTING_BEEP,
  SETTING_KEYLOCK,
  SETTING_PTT_LOCK,
  SETTING_BUSYCHANNELTXLOCK,
  SETTING_STE,
  SETTING_REPEATERSTE,
  SETTING_DTMFDECODE,
  SETTING_BRIGHTNESS_H,
  SETTING_BRIGHTNESS_L,
  SETTING_CONTRAST,
  SETTING_MAINAPP,
  SETTING_SQOPENEDTIMEOUT,
  SETTING_SQCLOSEDTIMEOUT,
  SETTING_SQLOPENTIME,
  SETTING_SQLCLOSETIME,
  SETTING_SKIPGARBAGEFREQUENCIES,
  SETTING_ACTIVEVFO,
  SETTING_BACKLIGHTONSQUELCH,
  SETTING_BATTERYCALIBRATION,
  SETTING_BATTERYTYPE,
  SETTING_BATTERYSTYLE,
  SETTING_UPCONVERTER,
  SETTING_DEVIATION,
  SETTING_SHOWLEVELINVFO,
  SETTING_BOUND240_280,
  SETTING_NOLISTEN,
  SETTING_SI4732POWEROFF,
  SETTING_TONELOCAL,
  SETTING_FCTIME,
  SETTING_MULTIWATCH,

  SETTING_COUNT,
} Setting;

typedef enum {
  BL_SQL_OFF,
  BL_SQL_ON,
  BL_SQL_OPEN,
} BacklightOnSquelchMode;

typedef enum {
  CH_DISPLAY_MODE_NF,
  CH_DISPLAY_MODE_F,
  CH_DISPLAY_MODE_N,
} CHDisplayMode;

typedef enum {
  MW_OFF,
  MW_ON,
  MW_SWITCH,
  MW_EXTRA,
} MultiwatchType;

typedef enum {
  BAT_1600,
  BAT_2200,
  BAT_3500,
} BatteryType;

typedef enum {
  BAT_CLEAN,
  BAT_PERCENT,
  BAT_VOLTAGE,
} BatteryStyle;

typedef enum {
  SCAN_TO_0,
  SCAN_TO_100ms,
  SCAN_TO_250ms,
  SCAN_TO_500ms,
  SCAN_TO_1s,
  SCAN_TO_2s,
  SCAN_TO_5s,
  SCAN_TO_10s,
  SCAN_TO_30s,
  SCAN_TO_1min,
  SCAN_TO_2min,
  SCAN_TO_5min,
  SCAN_TO_NONE,
} ScanTimeout;

typedef enum {
  EEPROM_BL24C64,   //
  EEPROM_BL24C128,  //
  EEPROM_BL24C256,  //
  EEPROM_BL24C512,  //
  EEPROM_BL24C1024, //
  EEPROM_M24M02,    //
  EEPROM_UNKNOWN,
} EEPROMType;

extern const char *EEPROM_TYPE_NAMES[6];
extern const uint32_t EEPROM_SIZES[6];
extern char *SCAN_TIMEOUT_NAMES[15];
extern uint32_t SCAN_TIMEOUTS[15];
extern const char *MW_NAMES[4];

typedef struct {
  uint32_t upconverter : 27;
  uint8_t checkbyte : 5;

  uint16_t currentScanlist;
  uint8_t mainApp : 8;

  uint16_t batteryCalibration : 12;
  uint8_t contrast : 4;

  uint8_t backlight : 4;
  uint8_t mic : 4;

  uint8_t reserved3 : 4;
  uint8_t batsave : 4;

  uint8_t vox : 4;
  uint8_t txTime : 4;

  uint8_t fcTime : 2;
  uint8_t reserved5 : 2;
  uint8_t iAmPro : 1;
  uint8_t roger : 3;

  uint8_t scanmode : 2;
  CHDisplayMode chDisplayMode : 2;
  uint8_t pttLock : 1;
  bool showLevelInVFO : 1;
  uint8_t beep : 1;
  uint8_t keylock : 1;

  uint8_t busyChannelTxLock : 1;
  uint8_t ste : 1;
  uint8_t repeaterSte : 1;
  uint8_t dtmfdecode : 1;
  uint8_t brightness : 4;

  uint8_t brightnessLow : 4;
  EEPROMType eepromType : 3;
  bool bound_240_280 : 1;

  ScanTimeout sqClosedTimeout : 4;
  ScanTimeout sqOpenedTimeout : 4;

  BatteryType batteryType : 2;
  BatteryStyle batteryStyle : 2;
  bool noListen : 1;
  bool si4732PowerOff : 1;
  uint8_t mWatch : 2;

  uint8_t reserved6;

  BacklightOnSquelchMode backlightOnSquelch : 2;
  bool toneLocal : 1;
  uint8_t sqlOpenTime : 3;
  uint8_t sqlCloseTime : 2;

  uint8_t deviation;

  uint8_t activeVFO : 2;
  bool skipGarbageFrequencies : 1;

} __attribute__((packed)) Settings;
// getsize(Settings)

#define SETTINGS_OFFSET (0)
#define SETTINGS_SIZE sizeof(Settings)

#define CH_SIZE sizeof(CH)
#define CHANNELS_OFFSET (SETTINGS_OFFSET + SETTINGS_SIZE)

#define PATCH1_SIZE 15832
#define PATCH3_SIZE 8840

#define PATCH_SIZE PATCH3_SIZE

// settings
// VFOs
// channel 1
// channel 2

extern Settings gSettings;
extern uint8_t BL_TIME_VALUES[7];
extern const char *BL_SQL_MODE_NAMES[3];
extern const char *CH_DISPLAY_MODE_NAMES[3];
extern const char *rogerNames[2];
extern const char *FC_TIME_NAMES[4];

void SETTINGS_Save();
void SETTINGS_Load();
void SETTINGS_DelayedSave();
uint32_t SETTINGS_GetFilterBound();
uint32_t SETTINGS_GetEEPROMSize();
uint16_t SETTINGS_GetPageSize();
bool SETTINGS_IsPatchPresent();

uint32_t SETTINGS_GetValue(Setting s);
void SETTINGS_SetValue(Setting s, uint32_t v);
const char *SETTINGS_GetValueString(Setting s);
void SETTINGS_IncDecValue(Setting s, bool inc);

void SETTINGS_UpdateSave();

#endif /* end of include guard: SETTINGS_H */
