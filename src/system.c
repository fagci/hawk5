#include "system.h"
#include "apps/apps.h"
#include "board.h"
#include "driver/backlight.h"
#include "driver/eeprom.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/uart.h"
#include "external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "helper/bands.h"
#include "helper/battery.h"
#include "helper/menu.h"
#include "misc.h"
#include "radio.h"
#include "scheduler.h"
#include "settings.h"
#include "ui/graphics.h"
#include "ui/statusline.h"
#include <string.h>

#define queueLen 20
#define itemSize sizeof(SystemMessages)

static uint8_t DEAD_BUF[] = {0xDE, 0xAD};

static char notificationMessage[16] = "";
static uint32_t notificationTimeoutAt;

static uint32_t secondTimer;
static uint32_t appsRenderTimer;
static uint32_t appsKeyboardTimer;

static uint32_t lastUartDataTime;

static bool isUartWaiting() {
  return lastUartDataTime && Now() - lastUartDataTime < 5000;
}

static void appRender() {
  if (!gRedrawScreen) {
    return;
  }

  if (Now() - appsRenderTimer < 40) {
    return;
  }

  appsRenderTimer = Now();
  gRedrawScreen = false;

  UI_ClearScreen();

  APPS_render();

  if (notificationMessage[0]) {
    FillRect(0, 32 - 5, 128, 9, C_FILL);
    PrintMediumEx(64, 32 + 2, POS_C, C_CLEAR, notificationMessage);
  }

  STATUSLINE_render(); // coz of APPS_render calls STATUSLINE_SetText

  ST7565_Blit();
}

static void systemUpdate() {
  // RADIO_Update();
  BATTERY_UpdateBatteryInfo();
  BACKLIGHT_Update();
}

static bool resetNeeded() {
  uint8_t buf[2];
  EEPROM_ReadBuffer(0, buf, 2);

  return memcmp(buf, DEAD_BUF, 2) == 0;
}

static void loadSettingsOrReset() {
  SETTINGS_Load();
  if (gSettings.batteryCalibration > 2154 ||
      gSettings.batteryCalibration < 1900) {
    gSettings.batteryCalibration = 0;
    EEPROM_WriteBuffer(0, DEAD_BUF, 2);
    NVIC_SystemReset();
  }
}

static bool checkKeylock(Key_State_t state, KEY_Code_t key) {
  if (state == KEY_LONG_PRESSED && key == KEY_F) {
    gSettings.keylock = !gSettings.keylock;
    SETTINGS_Save();
    gRedrawScreen = true;
    return true;
  }

  /* if (gSettings.keylock && state == KEY_LONG_PRESSED && key == KEY_8) {
    captureScreen();
    return true;
  } */

  if (gSettings.keylock &&
      (gSettings.pttLock
           ? true
           : (key != KEY_PTT && key != KEY_SIDE1 && key != KEY_SIDE2)) &&
      !(state == KEY_LONG_PRESSED && key == KEY_F)) {
    return true;
  }

  return false;
}

static void processKeyboard() {
  SystemMessages n = KEYBOARD_GetKey();
  // Process system notifications
  if (n.message == MSG_KEYPRESSED && !isUartWaiting()) {
    BACKLIGHT_On();

    if (checkKeylock(n.state, n.key)) {
      return;
    }

    if (APPS_key(n.key, n.state) || (MENU_IsActive() && n.key != KEY_EXIT)) {
      // LogC(LOG_C_BRIGHT_WHITE, "[SYS] Apps key");
      gRedrawScreen = true;
      appsRenderTimer = 0;
    } else {
      // LogC(LOG_C_BRIGHT_WHITE, "[SYS] Global key");
      if (n.key == KEY_MENU) {
        if (n.state == KEY_LONG_PRESSED) {
          APPS_run(APP_SETTINGS);
        } else if (n.state == KEY_RELEASED) {
          APPS_run(APP_APPS_LIST);
        }
      }
      if (n.key == KEY_EXIT) {
        if (n.state == KEY_RELEASED) {
          APPS_exit();
        }
      }
    }
  }
}

void initDisplay() {
  LogC(LOG_C_BRIGHT_WHITE, "DISPLAY");
  ST7565_Init(true);
  LogC(LOG_C_BRIGHT_WHITE, "BACKLIGHT");
  BACKLIGHT_Init();
}

void SYS_Main() {

  BATTERY_UpdateBatteryInfo();

  SystemMessages n = KEYBOARD_GetKey();
  if (resetNeeded() || n.key == KEY_EXIT) {
    initDisplay();
    gSettings.batteryCalibration = 2000;
    gSettings.backlight = 5;
    APPS_run(APP_RESET);
  } else {
    loadSettingsOrReset();
    BATTERY_UpdateBatteryInfo();

    initDisplay();

    // better UX
    STATUSLINE_render();
    ST7565_Blit();

    LogC(LOG_C_BRIGHT_WHITE, "LOAD BANDS");
    BANDS_Load();

    LogC(LOG_C_BRIGHT_WHITE, "RUN DEFAULT APP");
    APPS_run(gSettings.mainApp);
  }

  for (;;) {

    SETTINGS_UpdateSave();
    APPS_update();

    if (Now() - appsKeyboardTimer >= 10) {
      processKeyboard();
      appsKeyboardTimer = Now();
    }

    if (Now() - secondTimer >= 1000) {
      STATUSLINE_update();
      systemUpdate();
      secondTimer = Now();
    }

    appRender();

    while (gCurrentApp != APP_SCANER && UART_IsCommandAvailable()) {
      UART_HandleCommand();
      lastUartDataTime = Now();
    }
  }
}
