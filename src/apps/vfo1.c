#include "vfo1.h"
#include "../dcs.h"
#include "../driver/gpio.h"
#include "../driver/uart.h"
#include "../helper/bands.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../helper/regs-menu.h"
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

static char String[16];

static void setChannel(uint16_t v) {
  RADIO_LoadChannelToVFO(&gRadioState, RADIO_GetCurrentVFONumber(&gRadioState),
                         v);
}

static void tuneTo(uint32_t f, uint32_t _) {
  RADIO_SetParam(ctx, PARAM_FREQUENCY, f, true);
  RADIO_ApplySettings(ctx);
}

void VFO1_init(void) { gLastActiveLoot = NULL; }

static uint32_t lastRender;
static uint32_t lastSqCheck;

static const Step liveStep = STEP_5_0kHz;

void VFO1_update(void) {
  RADIO_UpdateMultiwatch(&gRadioState);
  RADIO_CheckAndSaveVFO(&gRadioState);

  if (!gSettings.mWatch && Now() - lastSqCheck >= SQL_DELAY) {
    RADIO_UpdateSquelch(&gRadioState);
    lastSqCheck = Now();
  }

  if (Now() - lastRender >= 1000) {
    lastRender = Now();
    gRedrawScreen = true;
  }
}

bool VFO1_key(KEY_Code_t key, Key_State_t state) {
  if (!gIsNumNavInput && state == KEY_RELEASED && REGSMENU_Key(key, state)) {
    return true;
  }

  uint8_t vfoN = RADIO_GetCurrentVFONumber(&gRadioState);

  if (state == KEY_RELEASED && vfo->mode == MODE_CHANNEL) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(vfo->channel_index, 0, CHANNELS_GetCountMax() - 1);
      gNumNavCallback = setChannel;
    }
    if (gIsNumNavInput) {
      NUMNAV_Input(key);
      return true;
    }
  }

  if (key == KEY_PTT && !gIsNumNavInput) {
    RADIO_ToggleTX(ctx, state == KEY_PRESSED);
    return true;
  }

  // pressed or hold continue
  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    const bool isSsb = RADIO_IsSSB(ctx);
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      if (vfo->mode == MODE_CHANNEL) {
        CHANNELS_Next(key == KEY_UP);
      } else {
        RADIO_IncDecParam(ctx, PARAM_FREQUENCY, key == KEY_UP, true);
      }
      // RADIO_SaveCurrentVFODelayed();
      return true;
    case KEY_SIDE1:
    case KEY_SIDE2:
      if (ctx->radio_type == RADIO_SI4732 && isSsb) {
        RADIO_AdjustParam(ctx, PARAM_FREQUENCY, key == KEY_SIDE1 ? 5 : -5,
                          true);
        // TODO: SAVE
        return true;
      }
      break;
    default:
      break;
    }
  }

  if (state == KEY_LONG_PRESSED) {
    switch (key) {
    case KEY_1:
      // gChListFilter = TYPE_FILTER_BAND;
      // APPS_run(APP_CH_LIST);
      return true;
    case KEY_2:
      if (gCurrentApp == APP_VFO1) {
        gSettings.iAmPro = !gSettings.iAmPro;
        SETTINGS_Save();
        return true;
      }
      return false;
    case KEY_3:
      RADIO_SaveCurrentVFO(&gRadioState);
      RADIO_ToggleVFOMode(&gRadioState, vfoN);
      // VFO1_init();
      return true;
    case KEY_4:
      gShowAllRSSI = !gShowAllRSSI;
      return true;
    case KEY_5:
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
    case KEY_STAR:
      // APPS_run(APP_SCANER);
      Log("------- REGS DUMP START -------");
      for (uint8_t i = 0; i < 128; ++i) {
        Log("%x, %u", i, BK4819_ReadRegister(i));
      }
      Log("------- REGS DUMP END -------");
      return true;
    case KEY_SIDE1:
    case KEY_SIDE2:
      SP_NextGraphUnit(key == KEY_SIDE1);
      return true;
    case KEY_EXIT:
      break;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
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
      RADIO_SaveVFOToStorage(&gRadioState,
                             RADIO_GetCurrentVFONumber(&gRadioState), &gChEd);
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
      break;
    case KEY_EXIT:
      if (!APPS_exit()) {
        RADIO_SaveCurrentVFO(&gRadioState);
        RADIO_SwitchVFO(&gRadioState,
                        IncDecU(vfoN, 0, gRadioState.num_vfos, true));
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

static void renderTxRxState(uint8_t y, bool isTx) {
  if (isTx && ctx->tx_state.is_active) {
    PrintMediumBoldEx(LCD_XCENTER, y, POS_C, C_FILL, "%s",
                      RADIO_GetParamValueString(ctx, PARAM_TX_STATE));
  }
}

static void renderChannelName(uint8_t y, uint16_t channel) {
  uint8_t vfoN = RADIO_GetCurrentVFONumber(&gRadioState);
  FillRect(0, y - 14, 30, 7, C_FILL);
  PrintSmallEx(15, y - 9, POS_C, C_INVERT, "VFO %u/%u", vfoN + 1,
               gRadioState.num_vfos);
  if (gRadioState.vfos[vfoN].mode == MODE_CHANNEL) {
    PrintSmallEx(32, y - 9, POS_L, C_FILL, "MR %03u", channel);
    UI_Scanlists(LCD_WIDTH - 25, y - 13, gSettings.currentScanlist);
  }
}

static void renderProModeInfo(uint8_t y) {
  if (ctx->radio_type == RADIO_BK4819) {
    PrintSmall(0, LCD_HEIGHT - 10, "R %+3u N %+3u G %+3u SNR %+2u",
               vfo->msm.rssi, vfo->msm.noise, vfo->msm.glitch, vfo->msm.snr);
  } else {
    PrintSmall(0, LCD_HEIGHT - 10, "R %+3u SNR %+2u", vfo->msm.rssi,
               vfo->msm.snr);
  }
}

#define AFC_COEFF_NUM 171 // 3.42 * 50 (упрощенный коэффициент)
#define AFC_COEFF_DEN 50 // Знаменатель
#define AFC_MODULO 65536

int32_t afc_to_deviation_hz(uint16_t afc_raw) {
  int32_t afc_signed = (int32_t)afc_raw;

  // Быстрое преобразование знака через маску
  if (afc_signed & 0x8000) { // Если старший бит установлен
    afc_signed |= 0xFFFF0000; // Расширяем знак (эквивалент вычитания 65536)
  }

  // Fixed-point умножение: (afc_signed * 171) / 50
  return (afc_signed * AFC_COEFF_NUM) / AFC_COEFF_DEN;
}

void VFO1_render(void) {
  const uint8_t BASE = 40;

  uint8_t vfoN = RADIO_GetCurrentVFONumber(&gRadioState);

  if (gIsNumNavInput) {
    STATUSLINE_SetText("Select: %s", gNumNavInput);
  } else if (gSettings.iAmPro &&
             (!gSettings.mWatch || vfo->is_open)) { // NOTE mwatch is temporary
    STATUSLINE_RenderRadioSettings();
  } else {
    STATUSLINE_SetText("Radio: %s",
                       RADIO_GetParamValueString(ctx, PARAM_RADIO));
  }

  uint32_t f =
      ctx->tx_state.is_active ? ctx->tx_state.frequency : ctx->frequency;
  const char *mod = RADIO_GetParamValueString(ctx, PARAM_MODULATION);

  if (vfo->mode == MODE_CHANNEL) {
    PrintMediumEx(LCD_XCENTER, BASE - 16, POS_C, C_FILL, "VFO %u", vfoN + 1);
  } else {
    if (gCurrentBand.meta.type == TYPE_BAND_DETACHED) {
      PrintSmallEx(32, 12, POS_L, C_FILL, "*%s", gCurrentBand.name);
    } else {
      PrintSmallEx(32, 12, POS_L, C_FILL, false ? "=%s:%u" : "%s:%u",
                   gCurrentBand.name,
                   CHANNELS_GetChannel(&gCurrentBand, ctx->frequency) + 1);
    }
  }

  int32_t khz_x10 = (afc_to_deviation_hz(BK4819_ReadRegister(0x6D)) + 50) /
                    100; // округление до 0.1kHz

  PrintSmallEx(0, 19, POS_L, C_FILL, "%+d.%dkHz", khz_x10 / 10,
               (khz_x10 > 0 ? 1 : -1) * khz_x10 % 10);
  /* PrintSmallEx(0, 24, POS_L, C_FILL, "POW %u",
               (BK4819_ReadRegister(0x7E) >> 6) & 0b111111); */

  // Шаг, полоса, уровень SQL, мощность, субтоны, названия каналов.

  renderTxRxState(BASE, ctx->tx_state.is_active);
  UI_BigFrequency(BASE, f);
  PrintMediumEx(LCD_WIDTH - 1, BASE - 12, POS_R, C_FILL, mod);
  renderChannelName(21, vfo->channel_index);
  const uint32_t step = StepFrequencyTable[ctx->step];
  /* if (potentialTxState == TX_ON) {
    PrintSmallEx(LCD_XCENTER, BASE + 6, POS_C, C_FILL, "%s",
                 TX_POWER_NAMES[radio.power]);
  } */
  PrintSmallEx(LCD_WIDTH, BASE + 6, POS_R, C_FILL, "%d.%02d", step / KHZ,
               step % KHZ);

  if (ctx->code.type) {
    PrintSmallEx(0, BASE - 6, POS_L, C_FILL, "R%s",
                 RADIO_GetParamValueString(ctx, PARAM_RX_CODE));
  }
  if (ctx->tx_state.code.type) {
    PrintSmallEx(0, BASE, POS_L, C_FILL, "T%s",
                 RADIO_GetParamValueString(ctx, PARAM_RX_CODE));
  }

  if (gSettings.iAmPro) {
    uint32_t lambda = 29979246 / (ctx->frequency / 100);
    PrintSmallEx(0, BASE - 14, POS_L, C_FILL, "L=%u/%ucm", lambda, lambda / 4);
  }

  if (gMonitorMode) {
    SPECTRUM_Y = BASE + 2;
    SPECTRUM_H = LCD_HEIGHT - SPECTRUM_Y;
    if (gSettings.showLevelInVFO) {
      char *graphMeasurementNames[] = {
          [GRAPH_RSSI] = "RSSI",           //
          [GRAPH_PEAK_RSSI] = "Peak RSSI", //
          [GRAPH_AGC_RSSI] = "AGC RSSI",   //
          [GRAPH_NOISE] = "Noise",         //
          [GRAPH_GLITCH] = "Glitch",       //
          [GRAPH_SNR] = "SNR",             //
      };
      switch (graphMeasurement) {
      case GRAPH_RSSI:
      case GRAPH_COUNT:
        SP_RenderGraph(RSSI_MIN, RSSI_MAX);
        break;
      case GRAPH_NOISE:
      case GRAPH_GLITCH:
        SP_RenderGraph(0, 256);
        break;
      case GRAPH_SNR:
        SP_RenderGraph(0, 30);
        break;
      case GRAPH_PEAK_RSSI:
        SP_RenderGraph(15, 88);
        break;
      case GRAPH_AGC_RSSI:
        SP_RenderGraph(25, 128);
        break;
      }
      PrintSmallEx(0, SPECTRUM_Y + 5, POS_L, C_FILL, "%s %+3u",
                   graphMeasurementNames[graphMeasurement],
                   SP_GetLastGraphValue());
    } else {
      UI_RSSIBar(BASE + 1);
    }
  } else {
    if (vfo->is_active || gSettings.iAmPro) {
      UI_RSSIBar(BASE + 1);
    }
    if (ctx->tx_state.is_active) {
      UI_TxBar(BASE + 1);
    }
    if (gSettings.iAmPro) {
      renderProModeInfo(BASE);
    }
  }

  if (gLastActiveLoot) {
    const uint32_t ago = (Now() - gLastActiveLoot->lastTimeOpen) / 1000;
    if (gLastActiveLoot->ct != 255) {
      // PrintRTXCode(String, CODE_TYPE_CONTINUOUS_TONE, gLastActiveLoot->ct);
      PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%s", String);
    } else if (gLastActiveLoot->cd != 255) {
      // PrintRTXCode(String, CODE_TYPE_DIGITAL, gLastActiveLoot->cd);
      PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%s", String);
    }
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, LCD_HEIGHT - 1, POS_C);
    if (ago) {
      PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 1, POS_R, C_FILL, "%u:%02u",
                   ago / 60, ago % 60);
    }
  }

  REGSMENU_Draw();
}
