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

static bool selStart = true;

static uint32_t cursorRangeTimeout = 0;

static bool isAnalyserMode = false;

static uint8_t scanAFC;
static uint32_t scanDelay;

static void setRange(uint32_t fs, uint32_t fe) {
  // radio.fixedBoundsMode = false;
  BANDS_RangeClear();
  SCAN_setStartF(fs);
  SCAN_setEndF(fe);
  BANDS_RangePush(gCurrentBand);
}

void SCANER_init(void) {
  gMonitorMode = false;
  // RADIO_ToggleRX(false); // In new radio done by default

  SPECTRUM_Y = 8;
  SPECTRUM_H = 44;

  gMonitorMode = false;

  if (gCurrentBand.meta.type != TYPE_BAND_DETACHED) {
    ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(&gRadioState);
    VFOContext *ctx = &vfo->context;
    BANDS_SelectByFrequency(RADIO_GetParam(ctx, PARAM_FREQUENCY), false);
    gCurrentBand.meta.type = TYPE_BAND_DETACHED;
  }

  BANDS_RangeClear(); // TODO: push only if gCurrentBand was changed from
                      // outside

  BANDS_RangePush(gCurrentBand);

  SCAN_Init(false);
}

void SCANER_update(void) { SCAN_Check(isAnalyserMode); }

bool SCANER_key(KEY_Code_t key, Key_State_t state) {
  if (REGSMENU_Key(key, state)) {
    return true;
  }
  ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(&gRadioState);
  VFOContext *ctx = &vfo->context;

  uint32_t step = RADIO_GetParam(ctx, PARAM_STEP);

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
    case KEY_5:
      selStart = !selStart;
      return true;
    case KEY_0:
      gChListFilter = TYPE_FILTER_BAND;
      APPS_run(APP_CH_LIST);
      return true;
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
      delay = AdjustU(delay, 0, 10000, key == KEY_1 ? 100 : -100);
      return true;
    case KEY_3:
    case KEY_9:
      RADIO_IncDecParam(ctx, PARAM_STEP, key == KEY_3, false);
      gCurrentBand.step = RADIO_GetParam(ctx, PARAM_STEP);
      SCAN_setBand(gCurrentBand);
      return true;
    case KEY_STAR:
      // APPS_run(APP_LOOT_LIST);
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
        delay = scanDelay;
        BK4819_SetAFC(scanAFC);
      } else {
        scanDelay = delay;
        scanAFC = BK4819_GetAFC();
        BK4819_SetAFC(0);
        delay = 0;
      }
      isAnalyserMode = !isAnalyserMode;
      minMaxRssi = SP_GetMinMax();
      return true;
    case KEY_5:
      gFInputCallback = setRange;
      FINPUT_setup(0, 1360 * MHZ, UNIT_MHZ, true);
      APPS_run(APP_FINPUT);
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      SCAN_Next(true);
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      SCAN_Next(true);
      return true;
    case KEY_STAR:
      // APPS_run(APP_LOOT_LIST);
      return true;

    case KEY_2:
      BANDS_RangePush(
          CUR_GetRange(BANDS_RangePeek(), RADIO_GetParam(ctx, PARAM_STEP)));
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
        APPS_run(APP_VFO1);
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
  ExtendedVFOContext *vfo = RADIO_GetCurrentVFO(&gRadioState);
  VFOContext *ctx = &vfo->context;

  const uint32_t step = RADIO_GetParam(ctx, PARAM_STEP);

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

  PrintSmallEx(0, 12, POS_L, C_FILL, "%uus", delay);

  PrintSmallEx(LCD_WIDTH, 12, POS_R, C_FILL, "%u.%02uk", step / 100,
               step % 100);
  if (BANDS_RangeIndex() > 0) {
    PrintSmallEx(0, 18, POS_L, C_FILL, "Zoom %u", BANDS_RangeIndex() + 1);
  }
  PrintSmallEx(0, 24, POS_L, C_FILL, "CPS %u", SCAN_GetCps());

  if (isAnalyserMode) {
    renderAnalyzerUI();
  } else {
    SP_RenderArrow(&gCurrentBand, RADIO_GetParam(ctx, PARAM_FREQUENCY));
  }

  // bottom
  Band r = CUR_GetRange(&gCurrentBand, step);
  bool showCurRange = Now() < cursorRangeTimeout;
  FSmall(1, LCD_HEIGHT - 2, POS_L, showCurRange ? r.rxF : gCurrentBand.rxF);
  FSmall(LCD_XCENTER, LCD_HEIGHT - 2, POS_C,
         showCurRange ? CUR_GetCenterF(&gCurrentBand, step)
                      : RADIO_GetParam(ctx, PARAM_FREQUENCY));
  FSmall(LCD_WIDTH - 1, LCD_HEIGHT - 2, POS_R,
         showCurRange ? r.txF : gCurrentBand.txF);

  FillRect(selStart ? 0 : LCD_WIDTH - 42, LCD_HEIGHT - 7, 42, 7, C_INVERT);

  CUR_Render();

  if (vfo->is_open) {
    UI_RSSIBar(16);
  }

  REGSMENU_Draw();
}

void SCANER_deinit(void) {}
