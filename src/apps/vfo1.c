#include "vfo1.h"
#include "../dcs.h"
#include "../driver/gpio.h"
#include "../driver/uart.h"
#include "../external/printf/printf.h"
#include "../helper/bands.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../helper/regs-menu.h"
#include "../helper/scan.h"
#include "../inc/dp32g030/gpio.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chcfg.h"
#include "chlist.h"
#include "finput.h"
#include <stdint.h>

static char String[16];
static const Step liveStep = STEP_5_0kHz;

static void updateBand(void) {
  uint32_t f = RADIO_GetParam(ctx, PARAM_FREQUENCY);
  if (!BANDS_InRange(f, gCurrentBand) ||
      gCurrentBand.meta.type == TYPE_BAND_DETACHED) {
    BANDS_SelectByFrequency(f, ctx->fixed_bounds);
  }
}

static void setChannel(uint16_t v) {
  RADIO_LoadChannelToVFO(gRadioState, RADIO_GetCurrentVFONumber(gRadioState),
                         v);
}

static void tuneTo(uint32_t f, uint32_t _) {
  (void)_;
  RADIO_SetParam(ctx, PARAM_FREQUENCY, f, true);
  RADIO_ApplySettings(ctx);
  updateBand();
}

void VFO1_init(void) {
  gLastActiveLoot = NULL;
  CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
  if (vfo->mode == MODE_CHANNEL) {
    setChannel(vfo->channel_index);
  }
  updateBand();

  SCAN_SetMode(SCAN_MODE_SINGLE);
  // SCAN_Init(false);
}

void VFO1_update(void) {}

static bool handleNumNav(KEY_Code_t key) {
  if (gIsNumNavInput) {
    NUMNAV_Input(key);
    return true;
  }

  if (key <= KEY_9) {
    NUMNAV_Init(vfo->channel_index, 0, CHANNELS_GetCountMax() - 1);
    gNumNavCallback = setChannel;
    return false;
  }

  return false;
}

static bool handleFrequencyChange(KEY_Code_t key) {
  if (key != KEY_UP && key != KEY_DOWN)
    return false;

  if (vfo->mode == MODE_CHANNEL) {
    CHANNELS_Next(key == KEY_UP);
  } else {
    RADIO_IncDecParam(ctx, PARAM_FREQUENCY, key == KEY_UP, true);
  }
  updateBand();
  return true;
}

static bool handleSSBFineTune(KEY_Code_t key) {
  if (ctx->radio_type != RADIO_SI4732 || !RADIO_IsSSB(ctx))
    return false;
  if (key != KEY_SIDE1 && key != KEY_SIDE2)
    return false;

  RADIO_AdjustParam(ctx, PARAM_FREQUENCY, key == KEY_SIDE1 ? 5 : -5, true);
  // TODO: SAVE
  return true;
}

static bool handleLongPress(KEY_Code_t key) {
  uint8_t vfoN = RADIO_GetCurrentVFONumber(gRadioState);

  switch (key) {
  case KEY_1:
    gChListFilter = TYPE_FILTER_BAND;
    APPS_run(APP_CH_LIST);
    return true;

  case KEY_2:
    if (gCurrentApp == APP_VFO1) {
      gSettings.iAmPro = !gSettings.iAmPro;
      SETTINGS_Save();
      return true;
    }
    return false;

  case KEY_3:
    RADIO_SaveCurrentVFO(gRadioState);
    RADIO_ToggleVFOMode(gRadioState, vfoN);
    updateBand();
    return true;

  case KEY_4:
    gShowAllRSSI = !gShowAllRSSI;
    return true;

  case KEY_5:
    RADIO_ToggleMultiwatch(gRadioState, !gRadioState->multiwatch_enabled);
    return true;

  case KEY_6:
    RADIO_IncDecParam(ctx, PARAM_POWER, true, true);
    return true;

  case KEY_7:
    RADIO_IncDecParam(ctx, PARAM_STEP, true, true);
    return true;

  case KEY_8:
    RADIO_IncDecParam(ctx, PARAM_TX_OFFSET, true, true);
    return true;

  case KEY_0:
    RADIO_IncDecParam(ctx, PARAM_MODULATION, true, true);
    return true;

  case KEY_SIDE1:
  case KEY_SIDE2:
    SP_NextGraphUnit(key == KEY_SIDE1);
    return true;

  default:
    return false;
  }
}

static bool handleRelease(KEY_Code_t key, Key_State_t state) {
  uint8_t vfoN = RADIO_GetCurrentVFONumber(gRadioState);

  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
    gFInputCallback = tuneTo;
    FINPUT_setup(0, BK4819_F_MAX, UNIT_MHZ, false);
    APPS_run(APP_FINPUT);
    APPS_key(key, state);
    return true;

  case KEY_F:
    RADIO_SaveVFOToStorage(gRadioState, vfoN, &gChEd);
    gChNum = -1;
    APPS_run(APP_CH_CFG);
    return true;

  case KEY_STAR:
    APPS_run(APP_LOOT_LIST);
    return true;

  case KEY_SIDE1:
    gMonitorMode = !gMonitorMode;
    return true;

  case KEY_SIDE2:
    GPIO_FlipBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
    return true;

  case KEY_EXIT:
    if (gMonitorMode) {
      gMonitorMode = false;
      return true;
    }
    if (!APPS_exit()) {
      RADIO_SaveCurrentVFO(gRadioState);
      RADIO_SwitchVFO(gRadioState,
                      IncDecU(vfoN, 0, gRadioState->num_vfos, true));
      updateBand();
    }
    return true;

  default:
    return false;
  }
}

bool VFO1_key(KEY_Code_t key, Key_State_t state) {
  if (REGSMENU_Key(key, state)) {
    return true;
  }

  // Обработка NUM NAV в режиме канала
  if (state == KEY_RELEASED && vfo->mode == MODE_CHANNEL && !gIsNumNavInput) {
    if (handleNumNav(key)) {
      return true;
    }
  }

  // PTT
  if (key == KEY_PTT && !gIsNumNavInput) {
    RADIO_ToggleTX(ctx, state != KEY_RELEASED);
    return true;
  }

  // Обработка нажатий и удержаний
  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    if (handleFrequencyChange(key))
      return true;
    if (handleSSBFineTune(key))
      return true;
  }

  // Длинные нажатия
  if (state == KEY_LONG_PRESSED) {
    return handleLongPress(key);
  }

  // Отпускания
  if (state == KEY_RELEASED) {
    return handleRelease(key, state);
  }

  return false;
}

static void renderTxRxState(uint8_t y, bool isTx) {
  if (isTx) {
    if (ctx->tx_state.is_active) {
      PrintMediumEx(0, 21, POS_L, C_FILL, "TX");
    } else {
      PrintMediumBoldEx(LCD_XCENTER, y, POS_C, C_FILL, "%s",
                        RADIO_GetParamValueString(ctx, PARAM_TX_STATE));
    }
  } else if (vfo->msm.open) {
    PrintMediumEx(0, 21, POS_L, C_FILL, "RX");
  }
}

static void renderChannelName(uint8_t y, uint16_t channel) {
  uint8_t vfoN = RADIO_GetCurrentVFONumber(gRadioState);
  FillRect(0, y - 14, 30, 7, C_FILL);
  PrintSmallEx(15, y - 9, POS_C, C_INVERT, "VFO %u/%u", vfoN + 1,
               gRadioState->num_vfos);
  if (gRadioState->vfos[vfoN].mode == MODE_CHANNEL) {
    PrintSmallEx(32, y - 9, POS_L, C_FILL, "MR %03u", channel);
    UI_Scanlists(LCD_WIDTH - 25, y - 13, gSettings.currentScanlist);
  }
}

int32_t afc_to_deviation_hz(uint16_t reg_6d) {
  // Коэффициент: 1000 Гц / 291 единиц ≈ 3.436 Hz/единица
  // Используем целочисленную арифметику: (signed_val * 1000) / 291
  // Для точности используем int64_t
  return ((int64_t)(int16_t)reg_6d * 1000LL) / 291LL;
}

static void renderStatusLine(void) {
  if (gIsNumNavInput) {
    return;
  }

  if (!gSettings.mWatch || vfo->is_open) {
    STATUSLINE_RenderRadioSettings();
  } else {
    STATUSLINE_SetText("Radio: %s",
                       RADIO_GetParamValueString(ctx, PARAM_RADIO));
  }
}

static void renderBandInfo(uint8_t BASE) {
  if (vfo->mode == MODE_CHANNEL) {
    PrintMediumEx(LCD_XCENTER, BASE - 16, POS_C, C_FILL, "%s", ctx->name);
  } else {
    const char *format =
        (gCurrentBand.meta.type == TYPE_BAND_DETACHED) ? "*%s" : "%s:%u";
    uint32_t channel = CHANNELS_GetChannel(&gCurrentBand, ctx->frequency) + 1;

    if (gCurrentBand.meta.type == TYPE_BAND_DETACHED) {
      PrintSmallEx(32, 12, POS_L, C_FILL, format, gCurrentBand.name);
    } else {
      PrintSmallEx(32, 12, POS_L, C_FILL, format, gCurrentBand.name, channel);
    }
  }
}

static void renderCodes(uint8_t BASE) {
  if (ctx->code.type) {
    PrintSmallEx(0, BASE - 6, POS_L, C_FILL, "R%s",
                 RADIO_GetParamValueString(ctx, PARAM_RX_CODE));
  }
  if (ctx->tx_state.code.type) {
    PrintSmallEx(0, BASE, POS_L, C_FILL, "T%s",
                 RADIO_GetParamValueString(ctx, PARAM_TX_CODE));
  }
}

static void renderExtraInfo(uint8_t BASE) {
  uint32_t txF = RADIO_GetParam(ctx, PARAM_TX_FREQUENCY_FACT);
  uint32_t rxF = RADIO_GetParam(ctx, PARAM_FREQUENCY);
  bool isTxFDifferent = (txF != rxF);

  int16_t afcVal = BK4819_GetAFCValue();
  if (afcVal != 0) {
    PrintSmallEx(14, 21, POS_L, C_FILL, "%+d", BK4819_GetAFCValue() * 10);
  }

  if (isTxFDifferent) {
    PrintSmallEx(LCD_XCENTER, BASE + 6, POS_C, C_FILL, "TX: %s",
                 RADIO_GetParamValueString(ctx, PARAM_TX_FREQUENCY_FACT));
  }
}

static void renderLootInfo(void) {
  if (!gLastActiveLoot)
    return;

  const uint32_t ago = (Now() - gLastActiveLoot->lastTimeOpen) / 1000;

  if (gLastActiveLoot->ct != 255) {
    PrintRTXCode(String, CODE_TYPE_CONTINUOUS_TONE, gLastActiveLoot->ct);
    PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%s", String);
  } else if (gLastActiveLoot->cd != 255) {
    PrintRTXCode(String, CODE_TYPE_DIGITAL, gLastActiveLoot->cd);
    PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%s", String);
  }

  UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, LCD_HEIGHT - 1, POS_C);

  if (ago) {
    PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 1, POS_R, C_FILL, "%u:%02u", ago / 60,
                 ago % 60);
  }

  if (gRadioState->multiwatch_enabled) {
    PrintMediumEx(LCD_XCENTER, LCD_HEIGHT - 9, POS_C, C_FILL, "M");
  }
}

static void renderMonitorMode(uint8_t BASE) {
  SPECTRUM_Y = BASE + 2;
  SPECTRUM_H = LCD_HEIGHT - SPECTRUM_Y;

  if (false && gSettings.showLevelInVFO) {
    static char *graphMeasurementNames[] = {
        [GRAPH_RSSI] = "RSSI",
        [GRAPH_NOISE] = "Noise",
        [GRAPH_GLITCH] = "Glitch",
        [GRAPH_SNR] = "SNR",
    };

    static const struct {
      uint8_t min;
      uint8_t max;
    } graphRanges[] = {
        [GRAPH_RSSI] = {RSSI_MIN, RSSI_MAX},
        [GRAPH_NOISE] = {0, 255},
        [GRAPH_GLITCH] = {0, 255},
        [GRAPH_SNR] = {0, 30},
        [GRAPH_COUNT] = {RSSI_MIN, RSSI_MAX},
    };

    SP_RenderGraph(graphRanges[graphMeasurement].min,
                   graphRanges[graphMeasurement].max);
    PrintSmallEx(0, SPECTRUM_Y + 5, POS_L, C_FILL, "%s %+3u",
                 graphMeasurementNames[graphMeasurement],
                 SP_GetLastGraphValue());
  } else {
    UI_RSSIBar(BASE + 1);
  }
}

void VFO1_render(void) {
  const uint8_t BASE = 40;

  renderStatusLine();

  uint32_t f = RADIO_GetParam(
      ctx, ctx->tx_state.is_active ? PARAM_TX_FREQUENCY_FACT : PARAM_FREQUENCY);
  const char *mod = RADIO_GetParamValueString(ctx, PARAM_MODULATION);

  renderBandInfo(BASE);
  renderTxRxState(BASE - 4,
                  ctx->tx_state.is_active || ctx->tx_state.last_error);

  if (!ctx->tx_state.last_error) {
    UI_BigFrequency(BASE, f);
  }

  PrintMediumEx(LCD_WIDTH - 1, BASE - 12, POS_R, C_FILL, mod);
  renderChannelName(21, vfo->channel_index);

  const uint32_t step = StepFrequencyTable[ctx->step];
  PrintSmallEx(LCD_WIDTH, BASE + 6, POS_R, C_FILL, "%d.%02d", step / KHZ,
               step % KHZ);

  renderCodes(BASE);
  renderExtraInfo(BASE);
  renderLootInfo();

  if (gMonitorMode) {
    renderMonitorMode(BASE);
  } else {
    if (vfo->msm.open) {
      UI_RSSIBar(BASE + 1);
    }
    if (ctx->tx_state.is_active) {
      UI_TxBar(BASE + 1);
    }
  }

  REGSMENU_Draw();
}
