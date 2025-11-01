#include "scaner.h"
#include "../driver/st7565.h"
#include "../driver/uart.h"
#include "../helper/bands.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../helper/regs-menu.h"
#include "../helper/scan.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chlist.h"
#include "finput.h"
#include <stdint.h>

static VMinMax minMaxRssi;

static uint32_t cursorRangeTimeout = 0;

static bool isAnalyserMode = false;

static uint8_t scanAFC;
// static uint32_t scanDelay;

static void setRange(uint32_t fs, uint32_t fe) {
  // radio.fixedBoundsMode = false;
  BANDS_RangeClear();
  SCAN_setRange(fs, fe);
  BANDS_RangePush(gCurrentBand);
}

void SCANER_init(void) {
  gMonitorMode = false;
  // RADIO_ToggleRX(false); // In new radio done by default

  SPECTRUM_Y = 8;
  SPECTRUM_H = 44;

  gMonitorMode = false;

  if (gCurrentBand.meta.type != TYPE_BAND_DETACHED) {
    LogC(LOG_C_BRIGHT_YELLOW, "[i] [SCAN] Init withOUT detached band");
    BANDS_SelectByFrequency(RADIO_GetParam(ctx, PARAM_FREQUENCY), false);
    gCurrentBand.meta.type = TYPE_BAND_DETACHED;
  } else {
    LogC(LOG_C_BRIGHT_YELLOW, "[i] [SCAN] Init with detached band");
    if (!gCurrentBand.rxF && !gCurrentBand.txF) {
      gCurrentBand = defaultBand;
    }
  }

  gCurrentBand.step = RADIO_GetParam(ctx, PARAM_STEP);

  BANDS_RangeClear(); // TODO: push only if gCurrentBand was changed from
                      // outside

  BANDS_RangePush(gCurrentBand);

  SCAN_Init(false);
}

static uint32_t lastRender;

void SCANER_update(void) {
  SCAN_Check(isAnalyserMode);

  if (Now() - lastRender >= 500) {
    lastRender = Now();
    gRedrawScreen = true;
  }
}

static bool pttWasLongPressed = false;
bool SCANER_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && REGSMENU_Key(key, state)) {
    return true;
  }

  uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];

  if (state == KEY_PRESSED && key == KEY_PTT) {
    pttWasLongPressed = false;
  }

  if (state == KEY_LONG_PRESSED) {
    Band _b;
    switch (key) {
    case KEY_6:
      if (gLastActiveLoot) {
        _b = gCurrentBand;
        _b.rxF = gLastActiveLoot->f - step * 64;
        _b.txF = _b.rxF + step * 128;
        BANDS_RangePush(_b);
        SCAN_setBand(*BANDS_RangePeek());
        CUR_Reset();
      }
      return true;
    case KEY_0:
      gChListFilter = TYPE_FILTER_BAND;
      APPS_run(APP_CH_LIST);
      return true;
    case KEY_PTT:
      pttWasLongPressed = true;
      if (gSettings.keylock) {
        LOOT_WhitelistLast();
        SCAN_Next();
        return true;
      }
      break;
    default:
      break;
    }
  }

  if (state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_2:
    case KEY_8:
      CUR_Size(key == KEY_2);
      cursorRangeTimeout = Now() + 2000;
      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_1:
    case KEY_7:
      SCAN_SetDelay(
          AdjustU(SCAN_GetDelay(), 0, 10000, key == KEY_1 ? 100 : -100));
      return true;
    case KEY_3:
    case KEY_9:
      RADIO_IncDecParam(ctx, PARAM_STEP, key == KEY_3, false);
      gCurrentBand.step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];
      SCAN_setBand(gCurrentBand);
      return true;
    case KEY_UP:
    case KEY_DOWN:
      CUR_Move(key == KEY_UP);
      cursorRangeTimeout = Now() + 2000;
      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_4:
      if (isAnalyserMode) {
        // delay = scanDelay;
        BK4819_SetAFC(scanAFC);
      } else {
        // scanDelay = delay;
        scanAFC = BK4819_GetAFC();
        BK4819_SetAFC(0);
        // delay = 0;
      }
      isAnalyserMode = !isAnalyserMode;
      minMaxRssi = SP_GetMinMax();
      return true;
    case KEY_5:
      gFInputCallback = setRange;
      FINPUT_setup(0, BK4819_F_MAX, UNIT_MHZ, true);
      APPS_run(APP_FINPUT);
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      SCAN_Next();
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      SCAN_Next();
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;

    case KEY_2:
      BANDS_RangePush(
          CUR_GetRange(BANDS_RangePeek(),
                       StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)]));
      SCAN_setBand(*BANDS_RangePeek());
      CUR_Reset();
      return true;
    case KEY_8:
      BANDS_RangePop();
      SCAN_setBand(*BANDS_RangePeek());
      CUR_Reset();
      return true;

    case KEY_PTT:
      if (gLastActiveLoot && !gSettings.keylock) {
        RADIO_SetParam(ctx, PARAM_FREQUENCY, gLastActiveLoot->f, true);
        RADIO_ApplySettings(ctx);
        RADIO_SaveCurrentVFO(gRadioState);
        APPS_run(APP_VFO1);
        return true;
      }
      if (gSettings.keylock && !pttWasLongPressed) {
        pttWasLongPressed = false;
        LOOT_BlacklistLast();
        SCAN_Next();
        return true;
      }
      break;
    default:
      break;
    }
  }

  return false;
}

static void renderAnalyzerUI() {
  VMinMax mm = SP_GetMinMax();
  PrintSmallEx(LCD_WIDTH, 18, POS_R, C_FILL, "%3u %+3d", mm.vMax,
               Rssi2DBm(mm.vMax));
  PrintSmallEx(LCD_WIDTH, 24, POS_R, C_FILL, "%3u %+3d", mm.vMin,
               Rssi2DBm(mm.vMin));
}

void SCANER_render(void) {
  const uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];

  STATUSLINE_RenderRadioSettings();

  if (isAnalyserMode) {
    minMaxRssi.vMin = 55;
    minMaxRssi.vMax = RSSI_MAX;
  } else {
    minMaxRssi = SP_GetMinMax();
  }

  SP_Render(&gCurrentBand, minMaxRssi);

  // top
  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, 14, POS_C);
  }

  PrintSmallEx(0, 12, POS_L, C_FILL, "%uus", SCAN_GetDelay());

  PrintSmallEx(LCD_WIDTH, 12, POS_R, C_FILL, "%u.%02uk", step / 100,
               step % 100);
  if (BANDS_RangeIndex() > 0) {
    PrintSmallEx(0, 18, POS_L, C_FILL, "Zoom %u", BANDS_RangeIndex() + 1);
  }
  PrintSmallEx(0, 24, POS_L, C_FILL, "CPS %u", SCAN_GetCps());

  if (isAnalyserMode) {
    renderAnalyzerUI();
  } else {
    SP_RenderArrow(RADIO_GetParam(ctx, PARAM_FREQUENCY));
  }

  // bottom
  Band r = CUR_GetRange(&gCurrentBand, step);
  bool showCurRange = Now() < cursorRangeTimeout;
  FSmall(1, LCD_HEIGHT - 2, POS_L, showCurRange ? r.rxF : gCurrentBand.rxF);
  FSmall(LCD_XCENTER, LCD_HEIGHT - 2, POS_C,
         showCurRange ? CUR_GetCenterF(step)
                      : RADIO_GetParam(ctx, PARAM_FREQUENCY));
  FSmall(LCD_WIDTH - 1, LCD_HEIGHT - 2, POS_R,
         showCurRange ? r.txF : gCurrentBand.txF);

  CUR_Render();

  if (vfo->is_open) {
    UI_RSSIBar(17);
  }

  REGSMENU_Draw();
}

void SCANER_deinit(void) {}
