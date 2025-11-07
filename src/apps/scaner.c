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
static uint32_t lastRender;
static bool isAnalyserMode = false;
static bool pttWasLongPressed = false;
static uint8_t scanAFC;

static void setRange(uint32_t fs, uint32_t fe) {
  BANDS_RangeClear();
  SCAN_setRange(fs, fe);
  BANDS_RangePush(gCurrentBand);
}

static void initBand(void) {
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
}

void SCANER_init(void) {
  gMonitorMode = false;
  SPECTRUM_Y = 8;
  SPECTRUM_H = 44;

  initBand();

  gCurrentBand.step = RADIO_GetParam(ctx, PARAM_STEP);
  BANDS_RangeClear();
  BANDS_RangePush(gCurrentBand);
  SCAN_Init(false);
}

void SCANER_update(void) {
  SCAN_Check(isAnalyserMode);

  if (Now() - lastRender >= 500) {
    lastRender = Now();
    gRedrawScreen = true;
  }
}

static bool handleLongPress(KEY_Code_t key) {
  uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];
  Band _b;
  
  switch (key) {
  case KEY_6:
    if (!gLastActiveLoot) return false;
    
    _b = gCurrentBand;
    _b.rxF = gLastActiveLoot->f - step * 64;
    _b.txF = _b.rxF + step * 128;
    BANDS_RangePush(_b);
    SCAN_setBand(*BANDS_RangePeek());
    CUR_Reset();
    return true;
    
  case KEY_0:
    gChListFilter = TYPE_FILTER_BAND;
    APPS_run(APP_CH_LIST);
    return true;
    
  case KEY_PTT:
    if (gSettings.keylock) {
      pttWasLongPressed = true;
      LOOT_WhitelistLast();
      SCAN_Next();
      return true;
    }
    return false;
    
  default:
    return false;
  }
}

static bool handleRepeatableKeys(KEY_Code_t key) {
  switch (key) {
  case KEY_1:
  case KEY_7:
    SCAN_SetDelay(AdjustU(SCAN_GetDelay(), 0, 10000, key == KEY_1 ? 100 : -100));
    return true;
    
  case KEY_3:
  case KEY_9:
    RADIO_IncDecParam(ctx, PARAM_STEP, key == KEY_3, false);
    gCurrentBand.step = RADIO_GetParam(ctx, PARAM_STEP);
    SCAN_setBand(gCurrentBand);
    return true;
    
  case KEY_UP:
  case KEY_DOWN:
    CUR_Move(key == KEY_UP);
    cursorRangeTimeout = Now() + 2000;
    return true;
    
  default:
    return false;
  }
}

static bool handleLongPressCont(KEY_Code_t key) {
  switch (key) {
  case KEY_2:
  case KEY_8:
    CUR_Size(key == KEY_2);
    cursorRangeTimeout = Now() + 2000;
    return true;
    
  default:
    return false;
  }
}

static void toggleAnalyserMode(void) {
  if (isAnalyserMode) {
    BK4819_SetAFC(scanAFC);
  } else {
    scanAFC = BK4819_GetAFC();
    BK4819_SetAFC(0);
  }
  isAnalyserMode = !isAnalyserMode;
  minMaxRssi = SP_GetMinMax();
}

static bool handlePTTRelease(void) {
  // Переход в VFO если не заблокирован и есть активный сигнал
  if (gLastActiveLoot && !gSettings.keylock) {
    RADIO_SetParam(ctx, PARAM_FREQUENCY, gLastActiveLoot->f, true);
    RADIO_ApplySettings(ctx);
    RADIO_SaveCurrentVFO(gRadioState);
    APPS_run(APP_VFO1);
    return true;
  }
  
  // Блокировка: короткое нажатие = blacklist
  if (gSettings.keylock && !pttWasLongPressed) {
    pttWasLongPressed = false;
    LOOT_BlacklistLast();
    SCAN_Next();
    return true;
  }
  
  return false;
}

static bool handleRelease(KEY_Code_t key) {
  uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];
  
  switch (key) {
  case KEY_4:
    toggleAnalyserMode();
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
    BANDS_RangePush(CUR_GetRange(BANDS_RangePeek(), step));
    SCAN_setBand(*BANDS_RangePeek());
    CUR_Reset();
    return true;
    
  case KEY_8:
    BANDS_RangePop();
    SCAN_setBand(*BANDS_RangePeek());
    CUR_Reset();
    return true;

  case KEY_PTT:
    return handlePTTRelease();
    
  default:
    return false;
  }
}

bool SCANER_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && REGSMENU_Key(key, state)) {
    return true;
  }

  if (state == KEY_PRESSED && key == KEY_PTT) {
    pttWasLongPressed = false;
  }

  if (state == KEY_LONG_PRESSED) {
    return handleLongPress(key);
  }

  if (state == KEY_LONG_PRESSED_CONT) {
    return handleLongPressCont(key);
  }

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    if (handleRepeatableKeys(key)) {
      return true;
    }
  }

  if (state == KEY_RELEASED) {
    return handleRelease(key);
  }

  return false;
}

static void renderAnalyzerUI(void) {
  VMinMax mm = SP_GetMinMax();
  PrintSmallEx(LCD_WIDTH, 18, POS_R, C_FILL, "%3u %+3d", mm.vMax, Rssi2DBm(mm.vMax));
  PrintSmallEx(LCD_WIDTH, 24, POS_R, C_FILL, "%3u %+3d", mm.vMin, Rssi2DBm(mm.vMin));
}

static void renderTopInfo(void) {
  const uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];
  
  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, 14, POS_C);
  }

  PrintSmallEx(0, 12, POS_L, C_FILL, "%uus", SCAN_GetDelay());
  PrintSmallEx(LCD_WIDTH, 12, POS_R, C_FILL, "%u.%02uk", step / 100, step % 100);
  
  if (BANDS_RangeIndex() > 0) {
    PrintSmallEx(0, 18, POS_L, C_FILL, "Zoom %u", BANDS_RangeIndex() + 1);
  }
  
  PrintSmallEx(0, 24, POS_L, C_FILL, "CPS %u", SCAN_GetCps());
}

static void renderBottomFreq(uint32_t step) {
  Band r = CUR_GetRange(&gCurrentBand, step);
  bool showCurRange = (Now() < cursorRangeTimeout);
  
  uint32_t leftF = showCurRange ? r.rxF : gCurrentBand.rxF;
  uint32_t centerF = showCurRange ? CUR_GetCenterF(step) : RADIO_GetParam(ctx, PARAM_FREQUENCY);
  uint32_t rightF = showCurRange ? r.txF : gCurrentBand.txF;
  
  FSmall(1, LCD_HEIGHT - 2, POS_L, leftF);
  FSmall(LCD_XCENTER, LCD_HEIGHT - 2, POS_C, centerF);
  FSmall(LCD_WIDTH - 1, LCD_HEIGHT - 2, POS_R, rightF);
}

void SCANER_render(void) {
  const uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];

  STATUSLINE_RenderRadioSettings();

  // Установка диапазона для спектра
  if (isAnalyserMode) {
    minMaxRssi.vMin = 55;
    minMaxRssi.vMax = RSSI_MAX;
  } else {
    minMaxRssi = SP_GetMinMax();
  }

  SP_Render(&gCurrentBand, minMaxRssi);

  renderTopInfo();

  if (isAnalyserMode) {
    renderAnalyzerUI();
  } else {
    SP_RenderArrow(RADIO_GetParam(ctx, PARAM_FREQUENCY));
  }

  renderBottomFreq(step);
  CUR_Render();

  if (vfo->is_open) {
    UI_RSSIBar(17);
  }

  REGSMENU_Draw();
}

void SCANER_deinit(void) {}
