#include "chscan.h"

#include "../driver/system.h"
#include "../driver/uart.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "apps.h"

CH activeCh;

static bool lastListenState;
static uint32_t timeout = 0;
static bool isWaiting;

static void loadCurrentCh() {
  RADIO_LoadChannelToVFO(&gRadioState, RADIO_GetCurrentVFONumber(&gRadioState),
                         CHANNELS_GetCurrentScanlistCH());
}

static void nextWithTimeout() {
  ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(&gRadioState);
  VFOContext *ctx = &vfo->context;
  if (lastListenState != vfo->is_open) {
    lastListenState = vfo->is_open;
    if (vfo->is_open) {
      loadCurrentCh();
      isWaiting = true;
    }
    SetTimeout(&timeout, vfo->is_open
                             ? SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]
                             : SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
  }

  if (CheckTimeout(&timeout)) {
    CHANNELS_Next(true);
    isWaiting = false;
    SetTimeout(&timeout, 0);
    return;
  }
}

void CHSCAN_init(void) {
  CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
  loadCurrentCh();
}

void CHSCAN_deinit(void) {}

static uint32_t lastSqCheck;

void CHSCAN_update(void) {
  RADIO_UpdateMultiwatch(&gRadioState);
  RADIO_CheckAndSaveVFO(&gRadioState);

  nextWithTimeout();
  SYS_DelayMs(SQL_DELAY);
  if (Now() - lastSqCheck >= 55) {
    RADIO_UpdateSquelch(&gRadioState);
    lastSqCheck = Now();
  }
  gRedrawScreen = true;
}

bool CHSCAN_key(KEY_Code_t key, Key_State_t state) {
  bool longHeld = state == KEY_LONG_PRESSED;
  bool simpleKeypress = state == KEY_RELEASED;
  if ((longHeld || simpleKeypress) && (key > KEY_0 && key < KEY_9)) {
    gSettings.currentScanlist = CHANNELS_ScanlistByKey(
        gSettings.currentScanlist, key, longHeld && !simpleKeypress);
    CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
    loadCurrentCh();
    SETTINGS_DelayedSave();
    isWaiting = false;
    return true;
  }
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      nextWithTimeout();
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      nextWithTimeout();
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      nextWithTimeout();
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    default:
      break;
    }
  }
  return false;
}

void CHSCAN_render(void) {
  if (gScanlistSize) {
    ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(&gRadioState);
    if (vfo->is_open) {
      PrintMediumBoldEx(LCD_XCENTER, 18, POS_C, C_FILL, "%s", activeCh.name);
    } else {
      PrintMediumEx(LCD_XCENTER, 18, POS_C, C_FILL,
                    isWaiting ? "Waiting..." : "Scanning...");
    }
  }

  UI_RenderScanScreen();
}
