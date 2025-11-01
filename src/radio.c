#include "radio.h"
#include "board.h"
#include "dcs.h"
#include "driver/audio.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/bk4819-regs.h"
#include "driver/bk4819.h"
#include "driver/si473x.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "helper/bands.h"
#include "helper/battery.h"
#include "helper/channels.h"
#include "helper/lootlist.h"
#include "helper/measurements.h"
#include "misc.h"
#include "scheduler.h"
#include "settings.h"
#include <stdint.h>
#include <string.h>

#define RADIO_SAVE_DELAY_MS 1000

// #define DEBUG_PARAMS 1

bool gShowAllRSSI = false;
bool gMonitorMode = false;

RadioState *gRadioState;
ExtendedVFOContext *vfo;
VFOContext *ctx;

const char *RADIO_NAMES[3] = {
    [RADIO_BK4819] = "BK4819",
    [RADIO_BK1080] = "BK1080",
    [RADIO_SI4732] = "SI4732",
};

const char *FILTER_NAMES[4] = {
    [FILTER_VHF] = "VHF",
    [FILTER_UHF] = "UHF",
    [FILTER_OFF] = "Off",
    [FILTER_AUTO] = "Auto",
};

const char *PARAM_NAMES[] = {
    [PARAM_FREQUENCY] = "f",                 //
    [PARAM_STEP] = "Step",                   //
    [PARAM_POWER] = "Power",                 //
    [PARAM_TX_OFFSET] = "TX offset",         //
    [PARAM_TX_OFFSET_DIR] = "TX offset dir", //
    [PARAM_MODULATION] = "Mod",              //
    [PARAM_SQUELCH_VALUE] = "SQ",            //
    [PARAM_SQUELCH_TYPE] = "SQ type",        //
    [PARAM_VOLUME] = "Volume",               //
    [PARAM_GAIN] = "Gain",                   //
    [PARAM_BANDWIDTH] = "BW",                //
    [PARAM_TX_STATE] = "TX state",           //
    [PARAM_RADIO] = "Radio",                 //
    [PARAM_RX_CODE] = "RX code",             //
    [PARAM_TX_CODE] = "TX code",             //
    [PARAM_RSSI] = "RSSI",                   //
    [PARAM_GLITCH] = "Glitch",               //
    [PARAM_NOISE] = "Noise",                 //
    [PARAM_SNR] = "SNR",                     //
    [PARAM_PRECISE_F_CHANGE] = "Precise f",  //

    [PARAM_TX_POWER] = "TX Power",
    [PARAM_TX_POWER_AMPLIFIER] = "TX PA",
    [PARAM_TX_FREQUENCY] = "TX f",
    [PARAM_TX_FREQUENCY_FACT] = "TX f fact",

    [PARAM_AFC] = "AFC",         //
    [PARAM_AFC_SPD] = "AFC SPD", //
    [PARAM_DEV] = "DEV",         //
    [PARAM_MIC] = "MIC",         //
    [PARAM_XTAL] = "XTAL",       //
    [PARAM_FILTER] = "Filter",   //
};

const char *TX_STATE_NAMES[7] = {
    [TX_UNKNOWN] = "TX Off",              //
    [TX_ON] = "TX On",                    //
    [TX_VOL_HIGH] = "CHARGING",           //
    [TX_BAT_LOW] = "BAT LOW",             //
    [TX_DISABLED] = "DISABLED",           //
    [TX_DISABLED_UPCONVERTER] = "UPCONV", //
    [TX_POW_OVERDRIVE] = "HIGH POW",      //
};

const char *MOD_NAMES_BK4819[8] = {
    [MOD_FM] = "FM",   //
    [MOD_AM] = "AM",   //
    [MOD_LSB] = "LSB", //
    [MOD_USB] = "USB", //
    [MOD_BYP] = "BYP", //
    [MOD_RAW] = "RAW", //
    [MOD_WFM] = "WFM", //
};

const char *MOD_NAMES_SI47XX[8] = {
    [SI47XX_AM] = "AM",
    [SI47XX_FM] = "FM",
    [SI47XX_LSB] = "LSB",
    [SI47XX_USB] = "USB",
};

const char *BW_NAMES_BK4819[10] = {
    [BK4819_FILTER_BW_6k] = "U6K",   //
    [BK4819_FILTER_BW_7k] = "U7K",   //
    [BK4819_FILTER_BW_9k] = "N9k",   //
    [BK4819_FILTER_BW_10k] = "N10k", //
    [BK4819_FILTER_BW_12k] = "W12k", //
    [BK4819_FILTER_BW_14k] = "W14k", //
    [BK4819_FILTER_BW_17k] = "W17k", //
    [BK4819_FILTER_BW_20k] = "W20k", //
    [BK4819_FILTER_BW_23k] = "W23k", //
    [BK4819_FILTER_BW_26k] = "W26k", //
};

const char *BW_NAMES_SI47XX[7] = {
    [SI47XX_BW_1_8_kHz] = "1.8k", //
    [SI47XX_BW_1_kHz] = "1k",     //
    [SI47XX_BW_2_kHz] = "2k",     //
    [SI47XX_BW_2_5_kHz] = "2.5k", //
    [SI47XX_BW_3_kHz] = "3k",     //
    [SI47XX_BW_4_kHz] = "4k",     //
    [SI47XX_BW_6_kHz] = "6k",     //
};

const char *BW_NAMES_SI47XX_SSB[6] = {
    [SI47XX_SSB_BW_0_5_kHz] = "0.5k", //
    [SI47XX_SSB_BW_1_0_kHz] = "1.0k", //
    [SI47XX_SSB_BW_1_2_kHz] = "1.2k", //
    [SI47XX_SSB_BW_2_2_kHz] = "2.2k", //
    [SI47XX_SSB_BW_3_kHz] = "3k",     //
    [SI47XX_SSB_BW_4_kHz] = "4k",     //
};

const char *FLT_BOUND_NAMES[2] = {"240MHz", "280MHz"};

const char *SQ_TYPE_NAMES[4] = {"RNG", "RG", "RN", "R"};

const uint16_t StepFrequencyTable[15] = {
    2,   5,   50,  100,

    250, 500, 625, 833, 900, 1000, 1250, 2500, 5000, 10000, 50000,
};

// Диапазоны для BK4819
static const FreqBand bk4819_bands[] = {
    {
        .min_freq = BK4819_F_MIN,
        .max_freq = BK4819_F_MAX,
        .available_mods = {MOD_FM, MOD_AM, MOD_LSB, MOD_USB},
        .available_bandwidths =
            {
                BK4819_FILTER_BW_6k,  //  "U 6K"
                BK4819_FILTER_BW_7k,  //  "U 7K",
                BK4819_FILTER_BW_9k,  //  "N 9k",
                BK4819_FILTER_BW_10k, //  "N 10k",
                BK4819_FILTER_BW_12k, //  "W 12k",
                BK4819_FILTER_BW_14k, //  "W 14k",
                BK4819_FILTER_BW_17k, //  "W 17k",
                BK4819_FILTER_BW_20k, //  "W 20k",
                BK4819_FILTER_BW_23k, //  "W 23k",
                BK4819_FILTER_BW_26k, //	"W 26k",
            },
    },
    // ... другие диапазоны
};

// Диапазоны для SI4732
static const FreqBand si4732_bands[] = {
    {
        .min_freq = SI47XX_F_MIN,
        .max_freq = SI47XX_F_MAX,
        .available_mods = {SI47XX_AM, SI47XX_LSB, SI47XX_USB},
        .available_bandwidths =
            {
                SI47XX_BW_1_kHz,
                SI47XX_BW_1_8_kHz,
                SI47XX_BW_2_kHz,
                SI47XX_BW_2_5_kHz,
                SI47XX_BW_3_kHz,
                SI47XX_BW_4_kHz,
                SI47XX_BW_6_kHz,
            },
    },
    {
        .min_freq = SI47XX_F_MIN,
        .max_freq = SI47XX_F_MAX,
        .available_mods = {SI47XX_LSB, SI47XX_USB},
        .available_bandwidths =
            {
                SI47XX_SSB_BW_0_5_kHz,
                SI47XX_SSB_BW_1_0_kHz,
                SI47XX_SSB_BW_1_2_kHz,
                SI47XX_SSB_BW_2_2_kHz,
                SI47XX_SSB_BW_3_kHz,
                SI47XX_SSB_BW_4_kHz,
            },
    },
    {
        .min_freq = SI47XX_FM_F_MIN,
        .max_freq = SI47XX_FM_F_MAX,
        .available_mods = {SI47XX_FM},
        .available_bandwidths = {},
    },
};

static bool RADIO_HasSi() { return BK1080_ReadRegister(1) != 0x1080; }

static uint32_t getRealTxFreq(const VFOContext *ctx) {
  switch (ctx->tx_state.offsetDirection) {
  case OFFSET_PLUS:
    return ctx->frequency + ctx->tx_state.frequency;
  case OFFSET_MINUS:
    return ctx->frequency - ctx->tx_state.frequency;
  case OFFSET_FREQ:
    return ctx->tx_state.frequency;
  default:
    break;
  }
  return ctx->frequency;
}

static void enableCxCSS(VFOContext *ctx) {
  switch (ctx->tx_state.code.type) {
  case CODE_TYPE_CONTINUOUS_TONE:
    BK4819_SetCTCSSFrequency(CTCSS_Options[ctx->tx_state.code.value]);
    break;
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(ctx->tx_state.code.type,
                                                 ctx->tx_state.code.value));
    break;
  default:
    BK4819_ExitSubAu();
    break;
  }
}

static void setupToneDetection(VFOContext *ctx) {
  BK4819_WriteRegister(BK4819_REG_7E, 0x302E); // DC flt BW 0=BYP
  uint16_t InterruptMask = BK4819_REG_3F_CxCSS_TAIL;
  if (gSettings.dtmfdecode) {
    BK4819_EnableDTMF();
    InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
  } else {
    BK4819_DisableDTMF();
  }
  switch (ctx->code.type) {
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    // Log("DCS on");
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(ctx->code.type, ctx->code.value));
    InterruptMask |= BK4819_REG_3F_CDCSS_FOUND | BK4819_REG_3F_CDCSS_LOST;
    break;
  case CODE_TYPE_CONTINUOUS_TONE:
    // Log("CTCSS on");
    BK4819_SetCTCSSFrequency(CTCSS_Options[ctx->code.value]);
    InterruptMask |= BK4819_REG_3F_CTCSS_FOUND | BK4819_REG_3F_CTCSS_LOST;
    break;
  default:
    // Log("STE on");
    BK4819_SetCTCSSFrequency(670);
    BK4819_SetTailDetection(550);
    break;
  }
  BK4819_WriteRegister(BK4819_REG_3F, InterruptMask);
}

static void sendEOT() {
  BK4819_ExitSubAu();
  switch (gSettings.roger) {
  case 1:
    BK4819_PlayRogerTiny();
    break;
  default:
    break;
  }
  if (gSettings.ste) {
    SYS_DelayMs(50);
    BK4819_GenTail(4);
    BK4819_WriteRegister(BK4819_REG_51, 0x9033);
    SYS_DelayMs(200);
  }
  BK4819_ExitSubAu();
}

static TXStatus checkTX(VFOContext *ctx) {
  if (gSettings.upconverter) {
    return TX_DISABLED_UPCONVERTER;
  }

  if (ctx->radio_type != RADIO_BK4819) {
    return TX_DISABLED;
  }

  /* Band txBand = BANDS_ByFrequency(txF);

  if (!txBand.allowTx && !(RADIO_IsChMode() && radio->allowTx)) {
    return TX_DISABLED;
  } */

  if (gBatteryPercent == 0) {
    return TX_BAT_LOW;
  }
  if (gChargingWithTypeC || gBatteryVoltage > 880) {
    return TX_VOL_HIGH;
  }
  return TX_ON;
}

static void toggleBK4819(bool on) {
  static bool bk4819_listen;
  if (bk4819_listen == on) {
    return;
  }
  bk4819_listen = on;

  Log("[RADIO] BK4819 audio=%u", on);
  if (on) {
    BK4819_ToggleAFDAC(true);
    BK4819_ToggleAFBit(true);
    SYS_DelayMs(8);
    AUDIO_ToggleSpeaker(true);
  } else {
    AUDIO_ToggleSpeaker(false);
    SYS_DelayMs(8);
    BK4819_ToggleAFDAC(false);
    BK4819_ToggleAFBit(false);
  }
}

static void rxTurnOff(Radio r) {
  switch (r) {
  case RADIO_BK4819:
    BK4819_Idle();
    break;
  case RADIO_BK1080:
    BK1080_Mute(true);
    break;
  case RADIO_SI4732:
    if (gSettings.si4732PowerOff) {
      SI47XX_PowerDown();
    } else {
      SI47XX_SetVolume(0);
    }
    break;
  default:
    break;
  }
}

static void rxTurnOn(const VFOContext *ctx) {
  switch (ctx->radio_type) {
  case RADIO_BK4819:
    BK4819_RX_TurnOn();
    break;
  case RADIO_BK1080:
    BK4819_Idle();
    BK1080_Mute(false);
    BK1080_Init(ctx->frequency, true);
    break;
  case RADIO_SI4732:
    BK4819_Idle();
    if (gSettings.si4732PowerOff || !isSi4732On) {
      if (RADIO_IsSSB(ctx)) {
        SI47XX_PatchPowerUp();
      } else {
        SI47XX_PowerUp();
      }
    } else {
      SI47XX_SetVolume(63);
    }
    break;
  default:
    break;
  }
}

static void toggleBK1080SI4732(bool on) {
  static bool bc_listen;
  if (bc_listen == on) {
    return;
  }
  bc_listen = on;
  Log("[RADIO] BK1080/SI audio=%u", on);
  if (on) {
    SYS_DelayMs(8);
    AUDIO_ToggleSpeaker(true);
  } else {
    AUDIO_ToggleSpeaker(false);
    SYS_DelayMs(8);
  }
}

// функция переключения аудио на конкретный VFO
void RADIO_SwitchAudioToVFO(RadioState *state, uint8_t vfo_index) {
  if (vfo_index >= state->num_vfos)
    return;

  const ExtendedVFOContext *vfo = &state->vfos[vfo_index];

  switch (vfo->context.radio_type) {
  case RADIO_BK4819:
    rxTurnOff(RADIO_HasSi() ? RADIO_SI4732 : RADIO_BK1080);
    toggleBK1080SI4732(false);
    break;
  case RADIO_SI4732:
    toggleBK4819(false);
    rxTurnOff(RADIO_BK4819);
    break;
  case RADIO_BK1080:
    toggleBK4819(false);
    rxTurnOff(RADIO_BK4819);
    break;
  default:
    break;
  }

  rxTurnOn(&vfo->context);

  switch (vfo->context.radio_type) {
  case RADIO_BK4819:
    toggleBK4819(vfo->is_open);
    break;
  case RADIO_SI4732:
  case RADIO_BK1080:
    toggleBK1080SI4732(vfo->is_open);
    break;
  default:
    break;
  }
  BOARD_ToggleGreen(vfo->is_open);
  if (vfo->is_open) {
    if (gSettings.backlightOnSquelch != BL_SQL_OFF) {
      BACKLIGHT_On();
    }
  } else {
    if (gSettings.backlightOnSquelch == BL_SQL_OPEN) {
      BACKLIGHT_Toggle(false);
    }
  }

  // Устанавливаем громкость для выбранного VFO
  // AUDIO_SetVolume(vfo->context.volume);
}

static bool setParamBK4819(VFOContext *ctx, ParamType p) {
  switch (p) {
  case PARAM_GAIN:
    BK4819_SetAGC(ctx->modulation != MOD_AM, ctx->gain);
    return true;
  case PARAM_BANDWIDTH:
    BK4819_SetFilterBandwidth(ctx->bandwidth);
    return true;
  case PARAM_SQUELCH_VALUE:
    BK4819_Squelch(ctx->squelch.value, gSettings.sqlOpenTime,
                   gSettings.sqlCloseTime);
    return true;
  case PARAM_SQUELCH_TYPE:
    BK4819_SquelchType(ctx->squelch.type);
    return true;
  case PARAM_MODULATION:
    BK4819_SetModulation(ctx->modulation);
    return true;
  case PARAM_FREQUENCY:
    if (ctx->filter == FILTER_AUTO) {
      Filter filter = (ctx->frequency < SETTINGS_GetFilterBound()) ? FILTER_VHF
                                                                   : FILTER_UHF;
      BK4819_SelectFilterEx(filter);
    }
    BK4819_TuneTo(ctx->frequency,
                  ctx->preciseFChange); // TODO: check if SetFreq needed
    return true;
  /* case PARAM_TX_POWER_AMPLIFIER:
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE,
                         ctx->tx_state.pa_enabled);
    return true;
  case PARAM_TX_POWER:
    BK4819_SetupPowerAmplifier(ctx->tx_state.power_level,
                               ctx->tx_state.frequency);
    return true; */
  case PARAM_AFC:
    BK4819_SetAFC(ctx->afc);
    return true;
  case PARAM_AFC_SPD:
    BK4819_SetAFCSpeed(ctx->afc_speed);
    return true;
  case PARAM_XTAL:
    BK4819_XtalSet(ctx->xtal);
    return true;
  case PARAM_FILTER: {
    Filter filter = ctx->filter;
    if (ctx->filter == FILTER_AUTO) {
      filter = (ctx->frequency < SETTINGS_GetFilterBound()) ? FILTER_VHF
                                                            : FILTER_UHF;
    }
    BK4819_SelectFilterEx(filter);
  }
    return true;
  case PARAM_MIC:
    BK4819_SetRegValue(RS_MIC, ctx->mic);
    return true;
  case PARAM_DEV:
    BK4819_SetRegValue(RS_DEV, ctx->dev);
    return true;
  case PARAM_VOLUME:
    BK4819_SetRegValue(RS_AF_DAC_GAIN,
                       ConvertDomain(ctx->volume, 0, 100, 0, 15));
    return true;
  case PARAM_RADIO:
  case PARAM_PRECISE_F_CHANGE:
  case PARAM_STEP:
  case PARAM_POWER:
  case PARAM_TX_OFFSET:
  case PARAM_TX_OFFSET_DIR:
  case PARAM_TX_STATE:
  case PARAM_TX_FREQUENCY:
  case PARAM_TX_FREQUENCY_FACT:
  case PARAM_TX_POWER:
  case PARAM_TX_POWER_AMPLIFIER:
  case PARAM_RX_CODE:
  case PARAM_TX_CODE:
  case PARAM_RSSI:
  case PARAM_NOISE:
  case PARAM_GLITCH:
  case PARAM_SNR:
  case PARAM_COUNT:
    break;
  }
  return false;
}

static bool setParamSI4732(VFOContext *ctx, ParamType p) {
  switch (p) {
  case PARAM_FREQUENCY:
    SI47XX_TuneTo(ctx->frequency);
    return true;
  case PARAM_MODULATION:
    SI47XX_SwitchMode((SI47XX_MODE)ctx->modulation);
    return true;
  case PARAM_GAIN:
    SI47XX_SetAutomaticGainControl(ctx->gain == 0,
                                   ctx->gain == 0 ? 0 : ctx->gain);
    return true;
  case PARAM_BANDWIDTH:
    if (RADIO_IsSSB(ctx)) {
      SI47XX_SetSsbBandwidth(ctx->bandwidth);
    } else {
      SI47XX_SetBandwidth(ctx->bandwidth, true);
    }
    return true;
  case PARAM_VOLUME:
    SI47XX_SetVolume(ctx->volume);
    return true;
  case PARAM_RADIO:
  case PARAM_PRECISE_F_CHANGE:
  case PARAM_STEP:
  case PARAM_POWER:
  case PARAM_SQUELCH_TYPE:
  case PARAM_SQUELCH_VALUE:
  case PARAM_TX_OFFSET:
  case PARAM_TX_OFFSET_DIR:
  case PARAM_TX_STATE:
  case PARAM_TX_FREQUENCY:
  case PARAM_TX_FREQUENCY_FACT:
  case PARAM_TX_POWER:
  case PARAM_TX_POWER_AMPLIFIER:
  case PARAM_RX_CODE:
  case PARAM_TX_CODE:
  case PARAM_AFC:
  case PARAM_AFC_SPD:
  case PARAM_DEV:
  case PARAM_MIC:
  case PARAM_XTAL:
  case PARAM_FILTER:
  case PARAM_RSSI:
  case PARAM_NOISE:
  case PARAM_GLITCH:
  case PARAM_SNR:
  case PARAM_COUNT:
    break;
  }
  return false;
}

static bool setParamBK1080(VFOContext *ctx, ParamType p) {
  if (p == PARAM_FREQUENCY) {
    BK1080_SetFrequency(ctx->frequency);
    return true;
  }
  return false;
}

uint16_t RADIO_GetRSSI(const VFOContext *ctx) {
  switch (ctx->radio_type) {
  case RADIO_BK4819:
    return BK4819_GetRSSI();
  case RADIO_BK1080:
    return gShowAllRSSI ? BK1080_GetRSSI() : 0;
  case RADIO_SI4732:
    if (gShowAllRSSI) {
      RSQ_GET();
      return rsqStatus.resp.RSSI;
    }
    return 0;
  default:
    return 0;
  }
}

uint8_t RADIO_GetSNR(const VFOContext *ctx) {
  switch (ctx->radio_type) {
  case RADIO_BK4819:
    return ConvertDomain(BK4819_GetSNR(), 24, 170, 0, 30);
  case RADIO_BK1080:
    return gShowAllRSSI ? BK1080_GetSNR() : 0;
  case RADIO_SI4732:
    if (gShowAllRSSI) {
      RSQ_GET();
      return rsqStatus.resp.SNR;
    }
    return 0;
  default:
    return 0;
  }
}

uint8_t RADIO_GetNoise(const VFOContext *ctx) {
  return ctx->radio_type == RADIO_BK4819 ? BK4819_GetNoise() : 0;
}

uint8_t RADIO_GetGlitch(const VFOContext *ctx) {
  return ctx->radio_type == RADIO_BK4819 ? BK4819_GetGlitch() : 0;
}

static void updateContext() {
  vfo = RADIO_GetCurrentVFO(gRadioState);
  ctx = &vfo->context;
}

// Инициализация VFO
void RADIO_Init(VFOContext *ctx, Radio radio_type) {
  memset(ctx, 0, sizeof(VFOContext));
  ctx->radio_type = radio_type;

  // Установка диапазона по умолчанию
  switch (radio_type) {
  case RADIO_BK4819:
    ctx->current_band = &bk4819_bands[0];
    ctx->frequency = 145500000; // 145.5 МГц (диапазон FM)
    ctx->gain = AUTO_GAIN_INDEX;
    // ctx->afc_speed = BK4819_GetAFCSpeed();

    /* ctx->xtal = BK4819_GetRegValue(RS_XTAL_MODE);
    ctx->afc = BK4819_GetAFC();
    ctx->dev = BK4819_GetRegValue(RS_DEV);
    ctx->mic = BK4819_GetRegValue(RS_MIC);
    */

    break;
  case RADIO_SI4732:
    ctx->current_band = &si4732_bands[0];
    ctx->frequency = 7100000; // 7.1 МГц (диапазон AM)
    break;
  default:
    break;
  }
}

// Проверка параметра для текущего диапазона
bool RADIO_IsParamValid(VFOContext *ctx, ParamType param, uint32_t value) {
  const FreqBand *band = ctx->current_band;

  if (!band)
    return false;

  switch (param) {
  case PARAM_FREQUENCY:
  case PARAM_TX_FREQUENCY:
    return (value >= band->min_freq && value <= band->max_freq);
  case PARAM_MODULATION:
    return value < ARRAY_SIZE(band->available_mods);
  case PARAM_BANDWIDTH:
    return value < ARRAY_SIZE(band->available_bandwidths);
  case PARAM_GAIN:
    if (ctx->radio_type == RADIO_BK4819) {
      return value < ARRAY_SIZE(GAIN_TABLE);
    }
    if (ctx->radio_type == RADIO_SI4732) {
      return value <= 27; // 0..26 + auto
    }
    return false;
    /* case PARAM_TX_POWER:
      return value <= 0xC0; */
  case PARAM_POWER:
    return value <= TX_POW_HIGH;
  default:
    return true; // Остальные параметры не зависят от диапазона
  }
}

// Установка параметра (всегда uint32_t!)
void RADIO_SetParam(VFOContext *ctx, ParamType param, uint32_t value,
                    bool save_to_eeprom) {
  if (!RADIO_IsParamValid(ctx, param, value)) {
    LogC(LOG_C_RED, "[RADIO] ERR: %-12s -> %u", PARAM_NAMES[param], value);
    return;
  }

  uint32_t old_value = RADIO_GetParam(ctx, param);

  switch (param) {
  case PARAM_TX_FREQUENCY_FACT:
  case PARAM_TX_STATE:
  case PARAM_RSSI:
  case PARAM_NOISE:
  case PARAM_GLITCH:
  case PARAM_SNR:
    return;

  case PARAM_RX_CODE:
    ctx->code.value = value;
    break;
  case PARAM_TX_CODE:
    ctx->tx_state.code.value = value;
    break;

  case PARAM_PRECISE_F_CHANGE:
    ctx->preciseFChange = value;
    break;
  case PARAM_STEP:
    ctx->step = (Step)value;
    break;
  case PARAM_TX_FREQUENCY:
  case PARAM_TX_OFFSET:
    ctx->tx_state.frequency = value;
    break;
  case PARAM_TX_OFFSET_DIR:
    ctx->tx_state.offsetDirection = value;
    break;
  case PARAM_POWER: {
    ctx->power = value;
    ctx->tx_state.power_level =
        BANDS_CalculateOutputPower(ctx->power, ctx->tx_state.frequency);

    ctx->tx_state.pa_enabled = true;
    ctx->dirty[PARAM_TX_POWER] = true;
    ctx->dirty[PARAM_TX_POWER_AMPLIFIER] = true;
  } break;
  case PARAM_TX_POWER:
    ctx->tx_state.power_level = value;
    break;
  case PARAM_TX_POWER_AMPLIFIER:
    ctx->tx_state.pa_enabled = value;
    break;

  case PARAM_FREQUENCY:
    ctx->frequency = value;
    break;
  case PARAM_MODULATION:
    ctx->modulation = (ModulationType)value;
    break;
  case PARAM_BANDWIDTH:
    ctx->bandwidth = (uint16_t)value;
    break;
  case PARAM_VOLUME:
    ctx->volume = (uint8_t)value;
    break;
  case PARAM_GAIN:
    ctx->gain = value;
    break;
  case PARAM_AFC:
    ctx->afc = value;
    break;
  case PARAM_AFC_SPD:
    ctx->afc_speed = value;
    break;
  case PARAM_DEV:
    ctx->dev = value;
    break;
  case PARAM_MIC:
    ctx->mic = value;
    break;
  case PARAM_XTAL:
    ctx->xtal = (XtalMode)value;
    break;
  case PARAM_FILTER:
    ctx->filter = (Filter)value;
    break;
  case PARAM_SQUELCH_VALUE:
    ctx->squelch.value = value;
    break;
  case PARAM_SQUELCH_TYPE:
    ctx->squelch.type = value;
    break;
  case PARAM_RADIO:
    ctx->radio_type = value;
    for (uint8_t i = 0; i < PARAM_COUNT; ++i) {
      ctx->dirty[i] = true;
    }
    break;
  case PARAM_COUNT:
    return;
  }

#ifdef DEBUG_PARAMS
  LogC(LOG_C_WHITE, "[SET] %-12s -> %s%s", PARAM_NAMES[param],
       RADIO_GetParamValueString(ctx, param), save_to_eeprom ? " [W]" : "");
#endif /* ifdef DEBUG_PARAMS */

  // TODO: make dirty only when changed.
  // but, potential BUG: param not applied when 0
  ctx->dirty[param] = true;

  // Если значение изменилось и требуется сохранение - устанавливаем флаг
  if (save_to_eeprom && (old_value != value)) {
    LogC(LOG_C_BRIGHT_YELLOW, "[RADIO] SAVE %s", PARAM_NAMES[param]);
    ctx->save_to_eeprom = true;
    ctx->last_save_time = Now();
  }
}

uint32_t RADIO_GetParam(const VFOContext *ctx, ParamType param) {
  switch (param) {
  case PARAM_RX_CODE:
    return ctx->code.value;
  case PARAM_TX_CODE:
    return ctx->tx_state.code.value;
  case PARAM_TX_OFFSET:
    return ctx->tx_state.frequency;
  case PARAM_TX_OFFSET_DIR:
    return ctx->tx_state.offsetDirection;
  case PARAM_TX_FREQUENCY:
    return ctx->tx_state.frequency;
  case PARAM_TX_FREQUENCY_FACT:
    return getRealTxFreq(ctx);
  case PARAM_TX_POWER_AMPLIFIER:
    return ctx->tx_state.pa_enabled;
  case PARAM_TX_POWER:
    return ctx->tx_state.power_level;
  case PARAM_TX_STATE:
    return ctx->tx_state.is_active;
  case PARAM_RSSI:
    return vfo->msm.rssi;
  case PARAM_NOISE:
    return vfo->msm.noise;
  case PARAM_GLITCH:
    return vfo->msm.glitch;
  case PARAM_SNR:
    return vfo->msm.snr;
  case PARAM_FREQUENCY:
    return ctx->frequency;
  case PARAM_PRECISE_F_CHANGE:
    return ctx->preciseFChange;
  case PARAM_MODULATION:
    return ctx->modulation;
  case PARAM_BANDWIDTH:
    return ctx->bandwidth;
  case PARAM_STEP:
    return ctx->step;
  case PARAM_VOLUME:
    return ctx->volume;
  case PARAM_GAIN:
    return ctx->gain;
  case PARAM_SQUELCH_TYPE:
    return ctx->squelch.type;
  case PARAM_SQUELCH_VALUE:
    return ctx->squelch.value;
  case PARAM_RADIO:
    return ctx->radio_type;
  case PARAM_POWER:
    return ctx->power;
  case PARAM_AFC:
    return ctx->afc;
  case PARAM_AFC_SPD:
    return ctx->afc_speed;
  case PARAM_XTAL:
    return ctx->xtal;
  case PARAM_FILTER:
    return ctx->filter;
  case PARAM_MIC:
    return ctx->mic;
  case PARAM_DEV:
    return ctx->dev;
  case PARAM_COUNT:
    break;
  }
  return 0;
}

bool RADIO_AdjustParam(VFOContext *ctx, ParamType param, uint32_t inc,
                       bool save_to_eeprom) {
  const FreqBand *band = ctx->current_band;
  if (!band) {
    return false;
  }

  uint32_t mi = 0, ma = UINT32_MAX, v = RADIO_GetParam(ctx, param);

  switch (param) {
  case PARAM_FREQUENCY:
    mi = band->min_freq;
    ma = band->max_freq;
    break;
  case PARAM_TX_OFFSET:
  case PARAM_TX_FREQUENCY:
    mi = band->min_freq;
    ma = band->max_freq;
    break;
  case PARAM_POWER:
    ma = 4;
    break;
  case PARAM_TX_POWER:
    ma = 255;
    break;
  case PARAM_TX_OFFSET_DIR:
    ma = OFFSET_FREQ + 1;
    break;
  case PARAM_TX_POWER_AMPLIFIER:
    ma = 2;
    break;
  case PARAM_MODULATION:
    ma = ARRAY_SIZE(band->available_mods);
    break;
  case PARAM_BANDWIDTH:
    ma = ARRAY_SIZE(band->available_bandwidths);
    break;
  case PARAM_STEP:
    ma = STEP_COUNT;
    break;
  case PARAM_SQUELCH_VALUE:
    ma = 11;
    break;
  case PARAM_SQUELCH_TYPE:
    ma = 4;
    break;
  case PARAM_AFC:
    ma = 9;
    break;
  case PARAM_AFC_SPD:
    ma = 64;
    break;
  case PARAM_DEV:
    ma = 2550;
    break;
  case PARAM_MIC:
    ma = 16;
    break;
  case PARAM_XTAL:
    ma = XTAL_3_38_4M + 1;
    break;
  case PARAM_FILTER:
    ma = FILTER_AUTO + 1;
    break;
  case PARAM_GAIN:
    if (ctx->radio_type == RADIO_BK4819) {
      ma = ARRAY_SIZE(GAIN_TABLE);
    } else if (ctx->radio_type == RADIO_SI4732) {
      ma = 28;
    } else {
      ma = 0;
    }
    break;
  case PARAM_RADIO:
    ma = 3;
    break;
  default:
    LogC(LOG_C_RED, "[RADIO] ERR: range %s", PARAM_NAMES[param]);
    return false;
  }
  RADIO_SetParam(ctx, param, AdjustU(v, mi, ma, inc), save_to_eeprom);
  RADIO_ApplySettings(ctx);
  return true;
}

bool RADIO_IncDecParam(VFOContext *ctx, ParamType param, bool inc,
                       bool save_to_eeprom) {
  uint32_t v = 1;
  if (param == PARAM_FREQUENCY) {
    v = StepFrequencyTable[ctx->step];
  }
  if (param == PARAM_DEV) {
    v = 10;
  }
  return RADIO_AdjustParam(ctx, param, inc ? v : -v, save_to_eeprom);
}

typedef bool (*spf_ptr)(VFOContext *, ParamType);

static spf_ptr setParamForRadio[] = {
    [RADIO_BK4819] = &setParamBK4819,
    [RADIO_BK1080] = &setParamBK1080,
    [RADIO_SI4732] = &setParamSI4732,
};

// Применение настроек
void RADIO_ApplySettings(VFOContext *ctx) {
  if (ctx->dirty[PARAM_RADIO]) {
    LogC(LOG_C_BRIGHT_MAGENTA, "[RADIO] =%s",
         RADIO_GetParamValueString(ctx, PARAM_RADIO));
    ctx->dirty[PARAM_RADIO] = false;
  }

  const bool needSetupToneDetection =
      (ctx->dirty[PARAM_RX_CODE] || ctx->dirty[PARAM_TX_CODE] ||
       ctx->dirty[PARAM_TX_STATE]) &&
      ctx->radio_type == RADIO_BK4819;

  for (uint8_t p = 0; p < PARAM_COUNT; ++p) {
    if (!ctx->dirty[p]) {
      continue;
    }

    switch (p) {
    case PARAM_STEP:
    case PARAM_POWER:
    case PARAM_TX_FREQUENCY:
    case PARAM_TX_OFFSET:
    case PARAM_TX_OFFSET_DIR:
    case PARAM_TX_STATE:
    case PARAM_TX_CODE:
    case PARAM_RX_CODE:
    case PARAM_RSSI:
    case PARAM_NOISE:
    case PARAM_GLITCH:
    case PARAM_SNR:
    case PARAM_PRECISE_F_CHANGE:
      ctx->dirty[p] = false;
      continue;
    }

    if (!setParamForRadio[ctx->radio_type](ctx, p)) {
#ifdef DEBUG_PARAMS
      LogC(LOG_C_YELLOW, "[W] Param %s not set for %s", PARAM_NAMES[p],
           RADIO_NAMES[ctx->radio_type]);
#endif
      continue;
    }
    ctx->dirty[p] = false;

#ifdef DEBUG_PARAMS
    LogC(LOG_C_BRIGHT_WHITE, "[SET] %-12s -> %s", PARAM_NAMES[p],
         RADIO_GetParamValueString(ctx, p));
#endif /* ifdef DEBUG_PARAMS */
  }

  if (needSetupToneDetection) {
    setupToneDetection(ctx);
  }
}

// Начать передачу
bool RADIO_StartTX(VFOContext *ctx) {
  TXStatus status = checkTX(ctx);
  if (status != TX_ON) {
    ctx->tx_state.last_error = status;
    return false;
  }
  ctx->tx_state.last_error = TX_UNKNOWN;
  if (ctx->tx_state.is_active) {
    return true;
  }

  uint8_t power = ctx->tx_state.power_level;

  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

  uint32_t txF = getRealTxFreq(ctx);
  BK4819_SelectFilter(txF);
  BK4819_TuneTo(txF, true);

  BOARD_ToggleRed(gSettings.brightness > 1);
  BK4819_PrepareTransmit();

  SYS_DelayMs(10);
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, ctx->tx_state.pa_enabled);
  SYS_DelayMs(5);
  BK4819_SetupPowerAmplifier(power, txF);
  SYS_DelayMs(10);

  enableCxCSS(ctx);
  ctx->tx_state.is_active = true;
  return true;
}

// Завершить передачу
void RADIO_StopTX(VFOContext *ctx) {
  ctx->tx_state.last_error = TX_UNKNOWN;
  if (!ctx->tx_state.is_active) {
    return;
  }

  BK4819_ExitDTMF_TX(true); // also prepares to tx ste

  sendEOT();

  ctx->tx_state.is_active = false;
  BOARD_ToggleRed(false);
  BK4819_TurnsOffTones_TurnsOnRX();

  BK4819_SetupPowerAmplifier(0, 0);
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

  setupToneDetection(ctx);
  BK4819_SelectFilter(ctx->frequency);
  BK4819_TuneTo(ctx->frequency, true);
}

void RADIO_ToggleTX(VFOContext *ctx, bool on) {
  if (on) {
    RADIO_StartTX(ctx);
  } else {
    RADIO_StopTX(ctx);
  }
}

bool RADIO_IsSSB(const VFOContext *ctx) {
  return ctx->modulation == MOD_LSB || ctx->modulation == MOD_USB;
}

// Initialize radio state
void RADIO_InitState(RadioState *state, uint8_t num_vfos) {
  Log("[RADIO] InitState");
  memset(state, 0, sizeof(RadioState));
  state->num_vfos = (num_vfos > MAX_VFOS) ? MAX_VFOS : num_vfos;

  state->primary_vfo_index = state->active_vfo_index = gSettings.activeVFO;

  for (uint8_t i = 0; i < state->num_vfos; i++) {
    RADIO_Init(&state->vfos[i].context, RADIO_BK4819); // Default to BK4819
    state->vfos[i].mode = MODE_VFO;
    state->vfos[i].channel_index = 0;
    state->vfos[i].is_active =
        (i == state->primary_vfo_index); // First VFO is active by default
    state->vfos[i].is_open = false;
  }

  updateContext();

  state->last_scan_time = 0;
  state->multiwatch_enabled = gSettings.mWatch;
}

// Функция проверки необходимости сохранения и собственно сохранения
void RADIO_CheckAndSaveVFO(RadioState *state) {
  for (uint8_t i = 0; i < state->num_vfos; ++i) {
    VFOContext *ctx = &state->vfos[i].context;
    uint16_t num = state->vfos[i].vfo_ch_index;

    if (ctx->save_to_eeprom &&
        (Now() - ctx->last_save_time >= RADIO_SAVE_DELAY_MS)) {
      LogC(LOG_C_BRIGHT_YELLOW, "TRYING TO SAVE PARAM");
      VFO ch;

      RADIO_SaveVFOToStorage(state, i, &ch);
      CHANNELS_Save(num, &ch);

      ctx->save_to_eeprom = false;
    }
  }
}

static bool RADIO_SwitchVFOTemp(RadioState *state, uint8_t vfo_index) {
  if (vfo_index >= state->num_vfos) {
    return false;
  }

  // Log("RADIO_SwitchVFOTemp %u", vfo_index);

  // Deactivate current VFO
  // state->vfos[state->active_vfo_index].is_active = false;

  VFOContext *oldCtx = &state->vfos[state->active_vfo_index].context;
  VFOContext *newCtx = &state->vfos[vfo_index].context;

  for (uint8_t p = 0; p < PARAM_COUNT; ++p) {
    newCtx->dirty[p] = RADIO_GetParam(oldCtx, p) != RADIO_GetParam(newCtx, p);
  }

  // mute previous vfo (fast fix)
  /* state->vfos[state->active_vfo_index].is_open = false;
  RADIO_SwitchAudioToVFO(state, state->active_vfo_index);
  RADIO_SwitchAudioToVFO(state, vfo_index); */

  // Activate new VFO
  state->vfos[vfo_index].is_active = true;
  state->active_vfo_index = vfo_index;
  gRedrawScreen = true;
  // updateContext();

  // Apply settings for the new VFO
  RADIO_ApplySettings(&state->vfos[vfo_index].context);

  return true;
}

// Switch to a different VFO
bool RADIO_SwitchVFO(RadioState *state, uint8_t vfo_index) {
  if (vfo_index >= state->num_vfos) {
    return false;
  }

  Log("[RADIO] SwitchVFO");

  RADIO_CheckAndSaveVFO(state);

  // Deactivate current VFO
  state->vfos[state->active_vfo_index].is_active = false;

  VFOContext *oldCtx = &state->vfos[state->active_vfo_index].context;
  VFOContext *newCtx = &state->vfos[vfo_index].context;

  for (uint8_t p = 0; p < PARAM_COUNT; ++p) {
    newCtx->dirty[p] = RADIO_GetParam(oldCtx, p) != RADIO_GetParam(newCtx, p);
  }

  // mute previous vfo (fast fix)
  state->vfos[state->active_vfo_index].is_open = false;
  RADIO_SwitchAudioToVFO(state, state->active_vfo_index);
  RADIO_SwitchAudioToVFO(state, vfo_index);

  // Activate new VFO
  state->vfos[vfo_index].is_active = true;
  state->primary_vfo_index = state->active_vfo_index = vfo_index;
  updateContext();

  // Apply settings for the new VFO
  RADIO_ApplySettings(&state->vfos[vfo_index].context);

  gSettings.activeVFO = vfo_index;
  SETTINGS_DelayedSave();

  return true;
}

static void setCommonParamsFromCh(VFOContext *ctx, const VFO *storage) {
  RADIO_SetParam(ctx, PARAM_BANDWIDTH, storage->bw, false);
  RADIO_SetParam(ctx, PARAM_FREQUENCY, storage->rxF, false);
  RADIO_SetParam(ctx, PARAM_GAIN, storage->gainIndex, false);
  RADIO_SetParam(ctx, PARAM_MODULATION, storage->modulation, false);
  RADIO_SetParam(ctx, PARAM_POWER, storage->power, false);
  RADIO_SetParam(ctx, PARAM_RADIO, storage->radio, false);
  RADIO_SetParam(ctx, PARAM_SQUELCH_TYPE, storage->squelch.type, false);
  RADIO_SetParam(ctx, PARAM_SQUELCH_VALUE, storage->squelch.value, false);
  RADIO_SetParam(ctx, PARAM_STEP, storage->step, false);
  RADIO_SetParam(ctx, PARAM_TX_FREQUENCY, storage->txF, false);
  RADIO_SetParam(ctx, PARAM_TX_OFFSET, storage->txF, false);
  RADIO_SetParam(ctx, PARAM_TX_OFFSET_DIR, storage->offsetDir, false);

  strncpy(ctx->name, storage->name, 9);

  ctx->code = storage->code.rx;
  ctx->tx_state.code = storage->code.tx;

  // Initialize TX state
  ctx->tx_state.is_active = false;
  ctx->tx_state.last_error = TX_UNKNOWN;

  RADIO_SetParam(ctx, PARAM_MIC, gSettings.mic, false);
  RADIO_SetParam(ctx, PARAM_DEV, gSettings.deviation * 10, false);
  RADIO_SetParam(ctx, PARAM_XTAL, XTAL_2_26M, false);
  RADIO_SetParam(ctx, PARAM_AFC, 8, false);
  RADIO_SetParam(ctx, PARAM_AFC_SPD, 63, false);
  RADIO_SetParam(ctx, PARAM_FILTER, FILTER_AUTO, false);

  RADIO_SetParam(ctx, PARAM_PRECISE_F_CHANGE, true, false);
  RADIO_SetParam(ctx, PARAM_VOLUME, 100, false);
}

// Load VFO settings from EEPROM storage
void RADIO_LoadVFOFromStorage(RadioState *state, uint8_t vfo_index,
                              const VFO *storage) {
  if (vfo_index >= state->num_vfos)
    return;
  LogC(LOG_C_BRIGHT_CYAN, "[RADIO] MEM -> VFO");

  ExtendedVFOContext *vfo = &state->vfos[vfo_index];
  vfo->mode = storage->isChMode;
  VFOContext *ctx = &vfo->context;

  // Set basic parameters
  setCommonParamsFromCh(ctx, storage);

  // Handle channel mode
  if (vfo->mode == MODE_CHANNEL) {
    vfo->channel_index = storage->channel;
    // Optionally validate the channel here
  }

  // RADIO_ApplySettings(&vfo->context);
}

// Save VFO settings to EEPROM storage
void RADIO_SaveVFOToStorage(const RadioState *state, uint8_t vfo_index,
                            VFO *storage) {
  if (vfo_index >= state->num_vfos)
    return;
  LogC(LOG_C_BRIGHT_CYAN, "[RADIO] VFO -> MEM");

  const ExtendedVFOContext *vfo = &state->vfos[vfo_index];
  const VFOContext *ctx = &vfo->context;

  storage->meta.type = TYPE_VFO;

  storage->isChMode = vfo->mode;
  storage->channel = vfo->channel_index;

  storage->radio = ctx->radio_type;

  storage->rxF = ctx->frequency;
  storage->step = ctx->step;

  storage->bw = ctx->bandwidth;
  storage->modulation = ctx->modulation;
  storage->gainIndex = ctx->gain;
  storage->squelch = ctx->squelch;

  storage->code.rx = ctx->code;
  storage->code.tx = ctx->tx_state.code;

  storage->txF = ctx->tx_state.frequency;
  storage->offsetDir = ctx->tx_state.offsetDirection;
}

bool RADIO_SaveCurrentVFO(RadioState *state) {
  if (!state)
    return false;

  uint8_t current = state->active_vfo_index;
  VFO storage;

  RADIO_SaveVFOToStorage(state, current, &storage);
  CHANNELS_Save(state->vfos[current].vfo_ch_index, &storage);

  return true;
}

// Load channel into VFO
void RADIO_LoadChannelToVFO(RadioState *state, uint8_t vfo_index,
                            uint16_t channel_index) {
  if (vfo_index >= state->num_vfos) {
    return;
  }

  LogC(LOG_C_BRIGHT_CYAN, "[RADIO] CH %u -> VFO", channel_index);

  ExtendedVFOContext *vfo = &state->vfos[vfo_index];
  VFOContext *ctx = &vfo->context;
  CH channel;
  CHANNELS_Load(channel_index, &channel);

  vfo->mode = MODE_CHANNEL;
  vfo->channel_index = channel_index;

  setCommonParamsFromCh(ctx, &channel);
}

/**
 * @brief Переключает режим работы VFO между частотным и канальным
 * @param state Указатель на состояние радио
 * @param vfo_index Индекс VFO (0..MAX_VFOS-1)
 * @return true если переключение успешно, false при ошибке
 */
bool RADIO_ToggleVFOMode(RadioState *state, uint8_t vfo_index) {
  // Проверка допустимости индекса VFO
  if (vfo_index >= state->num_vfos) {
    return false;
  }

  ExtendedVFOContext *vfo = &state->vfos[vfo_index];
  VFOContext *ctx = &vfo->context;

  // Определяем новый режим (инвертируем текущий)
  VFOMode new_mode = vfo->mode == MODE_CHANNEL ? MODE_VFO : MODE_CHANNEL;
  MR ch;
  CHANNELS_Load(vfo->vfo_ch_index, &ch);

  ch.isChMode = new_mode == MODE_CHANNEL;
  LogC(LOG_C_BRIGHT_CYAN, "[RADIO] VFOMode = %s", ch.isChMode ? "MR" : "VFO");

  CHANNELS_Save(vfo->vfo_ch_index, &ch);

  if (new_mode == MODE_CHANNEL) {
    RADIO_LoadChannelToVFO(state, vfo_index, ch.channel);
  } else {
    RADIO_LoadVFOFromStorage(state, vfo_index, &ch);
  }
  RADIO_ApplySettings(&vfo->context);

  // Помечаем для сохранения в EEPROM
  ctx->save_to_eeprom = true;
  ctx->last_save_time = Now();

  gRedrawScreen = true;

  return true;
}

// Toggle multiwatch on/off
void RADIO_ToggleMultiwatch(RadioState *state, bool enable) {
  if (state->multiwatch_enabled == enable) {
    return;
  }
  state->multiwatch_enabled = enable;
  if (!enable) {
    // Return to the primary VFO when disabling multiwatch
    RADIO_SwitchVFO(state, 0);
  }
}

// Check if a VFO is a broadcast receiver
static bool isBroadcastReceiver(const VFOContext *ctx) {
  return (ctx->radio_type == RADIO_SI4732 || ctx->radio_type == RADIO_BK1080);
}

bool RADIO_CheckSquelch(VFOContext *ctx) {
  if (gMonitorMode) {
    return true;
  }
  if (ctx->radio_type == RADIO_BK4819) {
    return BK4819_IsSquelchOpen();
  }

  return gShowAllRSSI ? RADIO_GetSNR(ctx) > ctx->squelch.value : true;
}

static void RADIO_UpdateMeasurement(ExtendedVFOContext *vfo) {
  // Log("Update MSM");
  VFOContext *ctx = &vfo->context;
  vfo->msm.f = ctx->frequency;
  vfo->msm.rssi = RADIO_GetRSSI(ctx);
  vfo->msm.snr = RADIO_GetSNR(ctx);
  vfo->msm.noise = BK4819_GetNoise();
  vfo->msm.glitch = BK4819_GetGlitch();
  vfo->msm.open = RADIO_CheckSquelch(ctx);
  if (!gMonitorMode) {
    LOOT_Update(&vfo->msm);
  }
}

void RADIO_UpdateSquelch(RadioState *state) {
  RADIO_UpdateMeasurement(&state->vfos[state->active_vfo_index]);
  if (vfo->msm.open != vfo->is_open) {
    gRedrawScreen = true; // TODO: mv
    RADIO_SetParam(ctx, PARAM_AFC_SPD, vfo->msm.open ? 57 : 63, false);
    RADIO_ApplySettings(ctx);
    vfo->is_open = vfo->msm.open;
    RADIO_SwitchAudioToVFO(state, state->active_vfo_index);
  }
}
// Update multiwatch state (should be called periodically)

void RADIO_UpdateMultiwatch(RadioState *state) {
  if (!state->multiwatch_enabled || gSettings.mWatch == 0) {
    state->scan_state = RADIO_SCAN_STATE_IDLE;
    return;
  }

  static int8_t current_scan_vfo = 0;
  static uint32_t last_scan_time = 0;
  uint32_t current_time = Now();

  switch (state->scan_state) {
  case RADIO_SCAN_STATE_IDLE:
    // Log("IDLE");
    // Начинаем новый цикл сканирования
    current_scan_vfo = -1;
    state->scan_state = RADIO_SCAN_STATE_SWITCHING;
    break;

  case RADIO_SCAN_STATE_SWITCHING:
    // RADIO_CheckAndSaveVFO(gRadioState);
    // Ищем следующий VFO для сканирования (пропускаем активный и вещательные)
    do {
      current_scan_vfo = (current_scan_vfo + 1) % state->num_vfos;
    } while (current_scan_vfo == state->active_vfo_index ||
             isBroadcastReceiver(&state->vfos[current_scan_vfo].context));

    // Временно переключаемся
    RADIO_SwitchVFOTemp(state, current_scan_vfo);
    // Log("SW %u", state->vfos[current_scan_vfo].context.frequency);
    state->scan_state = RADIO_SCAN_STATE_WARMUP;
    last_scan_time = current_time;
    break;

  case RADIO_SCAN_STATE_WARMUP:
    // Log("WU");
    // Ждем стабилизации сигнала
    if (last_scan_time && current_time - last_scan_time >= SQL_DELAY) {
      state->scan_state = RADIO_SCAN_STATE_MEASURING;
    }
    break;

  case RADIO_SCAN_STATE_MEASURING:
    // Log("ME");
    // Выполняем замер
    RADIO_UpdateMeasurement(&state->vfos[current_scan_vfo]);
    // Log("MSM: %u", state->vfos[current_scan_vfo].msm.open);
    state->scan_state = RADIO_SCAN_STATE_DECISION;
    break;

  case RADIO_SCAN_STATE_DECISION: {
    // Log("DEC");
    // Только для режима с автопереключением
    ExtendedVFOContext *scanned = &state->vfos[current_scan_vfo];
    ExtendedVFOContext *active = &state->vfos[state->active_vfo_index];
    if (gSettings.mWatch == 2) {

      // Условия переключения:
      bool should_switch =
          scanned->msm.open &&
          (scanned->msm.rssi > active->msm.rssi + 3 || // Гистерезис 3dB
           !active->msm.open) &&
          scanned->msm.rssi > 0;

      if (should_switch) {
        RADIO_SwitchVFO(state, current_scan_vfo);
      }
    } else {
      if (scanned->msm.open != scanned->is_open) {
        scanned->is_open = scanned->msm.open;
        RADIO_SwitchAudioToVFO(state, current_scan_vfo);
      }
      if (scanned->msm.open) {
        // Log("OPEN!!!");
        state->scan_state = RADIO_SCAN_STATE_WARMUP;
        last_scan_time = Now();
        return;
      }
    }

    // Переходим к следующему VFO или завершаем цикл
    if (current_scan_vfo >= state->num_vfos - 1) {
      last_scan_time = 0; // NOTE: IMPORTANT
      state->scan_state = RADIO_SCAN_STATE_IDLE;
    } else {
      state->scan_state = RADIO_SCAN_STATE_SWITCHING;
    }
    break;
  }
  }
}

void RADIO_LoadVFOs(RadioState *state) {
  Log("[RADIO] LoadVFOs");

  // TODO: init each radio separately
  BK4819_Init();
  BK4819_RX_TurnOn();

  BK1080_Init(0, false);

  uint8_t vfoIdx = 0;
  for (uint16_t i = 0; i < CHANNELS_GetCountMax(); ++i) {
    CHMeta meta = CHANNELS_GetMeta(i);

    bool isOurType = (TYPE_FILTER_VFO & (1 << meta.type)) != 0;
    if (!isOurType) {
      continue;
    }

    state->vfos[vfoIdx].vfo_ch_index = i;

    MR ch;
    CHANNELS_Load(i, &ch);
    if (ch.isChMode) {
      RADIO_LoadChannelToVFO(state, vfoIdx, ch.channel);
    } else {
      RADIO_LoadVFOFromStorage(state, vfoIdx, &ch);
    }
    vfoIdx++;
  }
  state->num_vfos = vfoIdx;

  VFOContext *ctx = &state->vfos[state->active_vfo_index].context;
  for (uint8_t p = 0; p < PARAM_COUNT; ++p) {
    ctx->dirty[p] = true;
  }

  RADIO_ApplySettings(ctx);

  updateContext();
}

// Включаем/выключаем маршрутизацию аудио
void RADIO_EnableAudioRouting(RadioState *state, bool enable) {
  state->audio_routing_enabled = enable;
  if (!enable) {
    // При выключении возвращаем аудио на активный VFO
    RADIO_SwitchAudioToVFO(state, state->active_vfo_index);
  }
}

// Обновление маршрутизации аудио
void RADIO_UpdateAudioRouting(RadioState *state) {
  if (!state->audio_routing_enabled)
    return;

  // Если текущий VFO - вещательный приемник, оставляем аудио на нем
  if (isBroadcastReceiver(&state->vfos[state->active_vfo_index].context)) {
    return;
  }

  // Проверяем активность на других VFO
  for (uint8_t i = 0; i < state->num_vfos; i++) {
    if (i == state->active_vfo_index)
      continue;

    ExtendedVFOContext *vfo = &state->vfos[i];

    // Для вещательных приемников не переключаем аудио автоматически
    if (isBroadcastReceiver(&vfo->context))
      continue;

    bool has_activity = RADIO_CheckSquelch(&vfo->context);

    if (has_activity) {
      // Если нашли активность и это не текущий VFO - переключаем аудио
      if (state->last_active_vfo != i) {
        RADIO_SwitchAudioToVFO(state, i);
        state->last_active_vfo = i;
      }
      return;
    }
  }

  // Если активность не обнаружена - возвращаем аудио на основной VFO
  if (state->last_active_vfo != state->active_vfo_index) {
    RADIO_SwitchAudioToVFO(state, state->active_vfo_index);
    state->last_active_vfo = state->active_vfo_index;
  }
}

static const char *RADIO_GetModulationName(const VFOContext *ctx) {
  if (ctx->radio_type == RADIO_BK4819) {
    return MOD_NAMES_BK4819[ctx->modulation];
  }
  if (ctx->radio_type == RADIO_SI4732) {
    return MOD_NAMES_SI47XX[ctx->modulation];
  }
  return "WFM";
}

const char *RADIO_GetParamValueString(const VFOContext *ctx, ParamType param) {
  static char buf[16] = "unk";
  uint32_t v = RADIO_GetParam(ctx, param);
  switch (param) {
  case PARAM_RSSI:
    snprintf(buf, 15, "%+ddB", Rssi2DBm(v));
    break;

  case PARAM_MODULATION:
    return RADIO_GetModulationName(ctx);
  case PARAM_TX_STATE:
    return TX_STATE_NAMES[ctx->tx_state.last_error];
  case PARAM_BANDWIDTH:
    if (ctx->radio_type == RADIO_BK4819) {
      return BW_NAMES_BK4819[v];
    }
    if (ctx->radio_type == RADIO_SI4732) {
      if (RADIO_IsSSB(ctx)) {
        return BW_NAMES_SI47XX_SSB[v];
      }
      return BW_NAMES_SI47XX[v];
    }
    return "?(WIP)";
  case PARAM_STEP:
    snprintf(buf, 15, "%d.%02d", StepFrequencyTable[v] / KHZ,
             StepFrequencyTable[v] % KHZ);
    break;
  case PARAM_FREQUENCY:
  case PARAM_TX_OFFSET:
  case PARAM_TX_FREQUENCY:
  case PARAM_TX_FREQUENCY_FACT:
    mhzToS(buf, v);
    break;
  case PARAM_RADIO:
    snprintf(buf, 15, "%s", RADIO_NAMES[ctx->radio_type]);
    break;
  case PARAM_TX_OFFSET_DIR:
    snprintf(buf, 15, "%s", TX_OFFSET_NAMES[ctx->radio_type]);
    break;
  case PARAM_GAIN:
    if (ctx->radio_type == RADIO_BK4819) {
      snprintf(buf, 15, v == AUTO_GAIN_INDEX ? "Auto" : "%+ddB",
               -GAIN_TABLE[v].gainDb + 33);
      break;
    } else if (ctx->radio_type == RADIO_SI4732) {
      snprintf(buf, 15, v == 0 ? "Auto" : "%u", v - 1);
      break;
    }
    snprintf(buf, 15, "Auto");
    break;

  case PARAM_RX_CODE:
    PrintRTXCode(buf, ctx->code.type, ctx->code.value);
    break;

  case PARAM_TX_CODE:
    PrintRTXCode(buf, ctx->tx_state.code.type, ctx->tx_state.code.value);
    break;

  case PARAM_POWER:
    return TX_POWER_NAMES[ctx->power];
  case PARAM_FILTER:
    return FILTER_NAMES[ctx->filter];
  case PARAM_NOISE:
  case PARAM_GLITCH:
  case PARAM_SNR:
  case PARAM_TX_POWER:
  case PARAM_TX_POWER_AMPLIFIER:
  case PARAM_AFC:
  case PARAM_AFC_SPD:
  case PARAM_DEV:
  case PARAM_MIC:
  case PARAM_XTAL:
  case PARAM_SQUELCH_VALUE:
  case PARAM_PRECISE_F_CHANGE:
  case PARAM_COUNT:
    snprintf(buf, 15, "%u", v);
    break;
  case PARAM_VOLUME:
    snprintf(buf, 15, "%u%", v);
    break;
  case PARAM_SQUELCH_TYPE:
    snprintf(buf, 15, "%s", SQ_TYPE_NAMES[ctx->squelch.type]);
    break;
  }
  return buf;
}

/**
 * @brief Получает номер текущего активного VFO
 * @param state Указатель на состояние радио
 * @return Номер VFO (0..MAX_VFOS-1) или 0xFF если не найдено
 */
uint8_t RADIO_GetCurrentVFONumber(const RadioState *state) {
  /* for (uint8_t i = 0; i < state->num_vfos; i++) {
    if (state->vfos[i].is_active) {
      return i;
    }
  }
  return 0xFF; // Ошибка - активный VFO не найден */
  return state->primary_vfo_index;
}

/**
 * @brief Получает указатель на текущий активный VFO
 * @param state Указатель на состояние радио
 * @return Указатель на ExtendedVFOContext или NULL если ошибка
 */
ExtendedVFOContext *RADIO_GetCurrentVFO(RadioState *state) {
  uint8_t current = RADIO_GetCurrentVFONumber(state);
  return (current != 0xFF) ? &state->vfos[current] : NULL;
}

/**
 * @brief Получает константный указатель на текущий активный VFO
 * @param state Указатель на состояние радио
 * @return Константный указатель на ExtendedVFOContext или NULL если ошибка
 */
const ExtendedVFOContext *RADIO_GetCurrentVFOConst(const RadioState *state) {
  uint8_t current = RADIO_GetCurrentVFONumber(state);
  return (current != 0xFF) ? &state->vfos[current] : NULL;
}
