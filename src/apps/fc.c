#include "fc.h"
#include "../dcs.h"
#include "../driver/system.h"
#include "../driver/uart.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../settings.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "apps.h"
#include <stdint.h>

static const uint8_t REQUIRED_FREQUENCY_HITS = 2;
static const uint8_t FILTER_SWITCH_INTERVAL = REQUIRED_FREQUENCY_HITS;

static const char *FILTER_NAMES[] = {
    [FILTER_OFF] = "ALL",
    [FILTER_VHF] = "VHF",
    [FILTER_UHF] = "UHF",
};
static Filter filter = FILTER_VHF;

static bool bandAutoSwitch = true;

static uint32_t bound;

static bool isScanning = true;
static uint32_t currentFrequency = 0;
static uint32_t lastDetectedFrequency = 0;
static uint8_t frequencyHits = 0;
static uint8_t filterSwitchCounter = 0;

static uint16_t hz = 0x244;

static uint32_t fcTimeuot;

static void enableScan() {
  Log("FC enable");
  BK4819_EnableFrequencyScanEx2(gSettings.fcTime, hz);
  isScanning = true;
}

static void disableScan() {
  Log("FC disable");
  BK4819_DisableFrequencyScan();
  BK4819_RX_TurnOn();
  isScanning = false;
}

void FC_init() {
  Log("FC init");
  bound = SETTINGS_GetFilterBound();
  BK4819_SelectFilterEx(filter);
  enableScan();
  frequencyHits = 0;
  filterSwitchCounter = 0;
}

void FC_deinit() { disableScan(); }

void switchFilter() {
  if (filter == FILTER_VHF) {
    filter = FILTER_UHF;
  } else {
    filter = FILTER_VHF;
  }

  BK4819_SelectFilterEx(filter);
  filterSwitchCounter = 0;
  gRedrawScreen = true;
}

void FC_update(void) {
  if (!CheckTimeout(&fcTimeuot)) {
    return;
  }

  ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(gRadioState);
  VFOContext *ctx = &vfo->context;

  RADIO_UpdateMultiwatch(gRadioState);
  RADIO_CheckAndSaveVFO(gRadioState);

  if (isScanning) {
    if (BK4819_GetFrequencyScanResult(&currentFrequency)) {
      // Log("FC got %u", currentFrequency);
      bool freqIsOk = true;

      gRedrawScreen = true;
      if (bandAutoSwitch) {
        filterSwitchCounter++;
      }

      if (currentFrequency >= 88 * MHZ && currentFrequency <= 108 * MHZ) {
        freqIsOk = false;
      }

      if (filter == FILTER_VHF && currentFrequency >= bound) {
        freqIsOk = false;
      }

      if (filter == FILTER_UHF && currentFrequency < bound) {
        freqIsOk = false;
      }

      if (freqIsOk && DeltaF(currentFrequency, lastDetectedFrequency) < 300) {
        frequencyHits++;
      } else {
        frequencyHits = 1;
      }
      // Log("FC hits %u", frequencyHits);

      lastDetectedFrequency = currentFrequency;

      if (frequencyHits >= REQUIRED_FREQUENCY_HITS) {
        // Log("FC hit! Tuning");
        disableScan();
        RADIO_SetParam(ctx, PARAM_FREQUENCY, currentFrequency, false);
        frequencyHits = 0;
      } else {
        disableScan();
        enableScan();
      }
    }

    if (filterSwitchCounter >= FILTER_SWITCH_INTERVAL) {
      // Log("FC switch filter");
      switchFilter();
    }
    SetTimeout(&fcTimeuot, 200 << gSettings.fcTime);
  } else {
    if (!vfo->is_open) {
      SYS_DelayMs(SQL_DELAY);
    }
    // Log("FC checklisten");
    RADIO_UpdateSquelch(gRadioState);

    if (vfo->is_open) {
      gRedrawScreen = true;
    } else {
      enableScan();
    }
  }
}

bool FC_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_5:
      hz = hz == 0x244 ? 0x580 : 0x244;
      break;
    default:
      break;
    }
  }
  ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(gRadioState);
  VFOContext *ctx = &vfo->context;

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_1:
    case KEY_7:
      gSettings.fcTime = IncDecI(gSettings.fcTime, 0, 3 + 1, key == KEY_1);
      SETTINGS_DelayedSave();
      FC_init();
      break;
    case KEY_3:
    case KEY_9:
      RADIO_IncDecParam(ctx, PARAM_SQUELCH_VALUE, key == KEY_3, true);
      break;
    case KEY_STAR:
      // APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      return true;
    case KEY_F:
      switchFilter();
      return true;
    case KEY_0:
      bandAutoSwitch = !bandAutoSwitch;
      return true;
    case KEY_PTT:
      if (gLastActiveLoot) {
        FC_deinit();
        RADIO_SetParam(ctx, PARAM_FREQUENCY, gLastActiveLoot->f, true);
        APPS_run(APP_VFO1);
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

void FC_render() {
  ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(gRadioState);
  VFOContext *ctx = &vfo->context;
  PrintMediumEx(0, 16, POS_L, C_FILL, "%s %ums HZ %u SQ %u %s",
                FILTER_NAMES[filter], 200 << gSettings.fcTime, hz,
                RADIO_GetParam(ctx, PARAM_SQUELCH_VALUE),
                bandAutoSwitch ? "[A]" : "");
  UI_BigFrequency(40, currentFrequency);

  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_WIDTH, 48, POS_R);

    if (gLastActiveLoot->ct != 0xFF) {
      PrintSmallEx(LCD_WIDTH, 40 + 8 + 6, POS_R, C_FILL, "CT:%u.%uHz",
                   CTCSS_Options[gLastActiveLoot->ct] / 10,
                   CTCSS_Options[gLastActiveLoot->ct] % 10);
    } else if (gLastActiveLoot->cd != 0xFF) {
      PrintSmallEx(LCD_WIDTH, 40 + 8 + 6, POS_R, C_FILL, "DCS:D%03oN",
                   DCS_Options[gLastActiveLoot->cd]);
    }
  }
}
