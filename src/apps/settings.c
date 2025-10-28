#include "settings.h"
#include "../driver/backlight.h"
#include "../driver/uart.h"
#include "../external/printf/printf.h"
#include "../helper/battery.h"
#include "../helper/menu.h"
#include "../misc.h"
#include "../settings.h"
#include "apps.h"
#include "finput.h"

static uint8_t DEAD_BUF[] = {0xDE, 0xAD};

static void getValS(const MenuItem *item, char *buf, uint8_t buf_size) {
  snprintf(buf, buf_size, "%s", SETTINGS_GetValueString(item->setting));
}

static void updateValS(const MenuItem *item, bool up) {
  SETTINGS_IncDecValue(item->setting, up);
}

static void doCalibrate(uint32_t v, uint32_t _) {
  SETTINGS_SetValue(SETTING_BATTERYCALIBRATION, BATTERY_GetCal(v * 100));
}

static bool calibrate(const MenuItem *item, KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && key == KEY_MENU) {
    gFInputValue1 = BATTERY_GetPreciseVoltage(
                        SETTINGS_GetValue(SETTING_BATTERYCALIBRATION)) /
                    100;
    gFInputCallback = doCalibrate;
    FINPUT_setup(500, 860, UNIT_VOLTS, false);
    APPS_run(APP_FINPUT);
    return true;
  }
  return false;
}

static const MenuItem sqlMenuItems[] = {
    {"Open t", SETTING_SQLOPENTIME, getValS, updateValS},
    {"Close t", SETTING_SQLCLOSETIME, getValS, updateValS},
};

static Menu sqlMenu = {"SQL", sqlMenuItems, ARRAY_SIZE(sqlMenuItems)};

static const MenuItem scanMenuItems[] = {
    {"SQL", .submenu = &sqlMenu},
    {"FC t", SETTING_FCTIME, getValS, updateValS},
    {"Listen t/o", SETTING_SQOPENEDTIMEOUT, getValS, updateValS},
    {"Stay t", SETTING_SQCLOSEDTIMEOUT, getValS, updateValS},
    {"Skip X_X", SETTING_SKIPGARBAGEFREQUENCIES, getValS, updateValS},
};

static Menu scanMenu = {"Scan", scanMenuItems, ARRAY_SIZE(scanMenuItems)};

static const MenuItem displayMenuItems[] = {
    {"Contrast", SETTING_CONTRAST, getValS, updateValS},
    {"BL max", SETTING_BRIGHTNESS_H, getValS, updateValS},
    {"BL min", SETTING_BRIGHTNESS_L, getValS, updateValS},
    {"BL time", SETTING_BACKLIGHT, getValS, updateValS},
    {"BL SQL mode", SETTING_BACKLIGHTONSQUELCH, getValS, updateValS},
    {"CH display", SETTING_CHDISPLAYMODE, getValS, updateValS},
};

static Menu displayMenu = {"Display", displayMenuItems,
                           ARRAY_SIZE(displayMenuItems)};

static const MenuItem radioMenuItems[] = {
    {"DTMF decode", SETTING_DTMFDECODE, getValS, updateValS},
    {"Filter bound", SETTING_BOUND240_280, getValS, updateValS},
    {"SI power off", SETTING_SI4732POWEROFF, getValS, updateValS},
    {"STE", SETTING_STE, getValS, updateValS},
    {"Tone local", SETTING_TONELOCAL, getValS, updateValS},
    {"Roger", SETTING_ROGER, getValS, updateValS},
    {"Multiwatch", SETTING_MULTIWATCH, getValS, updateValS},
    {"Mic", SETTING_MIC, getValS, updateValS},
    {"Freq corr", SETTING_FREQ_CORRECTION, getValS, updateValS},
};

static Menu radioMenu = {"Radio", radioMenuItems, ARRAY_SIZE(radioMenuItems)};

static const MenuItem batMenuItems[] = {
    {"Bat type", SETTING_BATTERYTYPE, getValS, updateValS},
    {"Bat style", SETTING_BATTERYSTYLE, getValS, updateValS},
    {"BAT cal", SETTING_BATTERYCALIBRATION, getValS, .action = calibrate},
};

static Menu batteryMenu = {"Battery", batMenuItems, ARRAY_SIZE(batMenuItems)};

static const MenuItem menuItems[] = {
    {"Scan", .submenu = &scanMenu},
    {"Radio", .submenu = &radioMenu},
    {"Display", .submenu = &displayMenu},
    {"Battery", .submenu = &batteryMenu},
    {"Beep", SETTING_BEEP, getValS, updateValS},
    {"Main app", SETTING_MAINAPP, getValS, updateValS},
    {"Lock PTT", SETTING_PTT_LOCK, getValS, updateValS},
    /*
    {"Upconv", M_UPCONVERTER, 0},
    {"Level in VFO", M_LEVEL_IN_VFO, 2},
    {"Mutliwatch", M_MWATCH, 4},
    {"Reset", M_RESET, 2}, */
};

static Menu settingsMenu = {
    .title = "Settings",
    .items = menuItems,
    .num_items = ARRAY_SIZE(menuItems),
};

void SETTINGS_init(void) { MENU_Init(&settingsMenu); }
void SETTINGS_deinit(void) {}

bool SETTINGS_key(KEY_Code_t key, Key_State_t state) {
  if (MENU_HandleInput(key, state)) {
    return true;
  }
  return false;
}

void SETTINGS_render(void) { MENU_Render(); }
