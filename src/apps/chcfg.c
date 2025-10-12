#include "chcfg.h"
#include "../dcs.h"
#include "../driver/uart.h"
#include "../external/printf/printf.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include "../helper/menu.h"
#include "../misc.h"
#include "../radio.h"
#include "apps.h"
#include "chlist.h"
#include "finput.h"
#include "textinput.h"
#include <string.h>

static const char *YES_NO[] = {"No", "Yes"};

MR gChEd;
int16_t gChNum = -1;

typedef enum {
  MEM_BOUNDS,
  MEM_START,
  MEM_END,
  MEM_BANK,
  MEM_BW,
  MEM_TYPE,
  MEM_TX_OFFSET,
  MEM_F_RX,
  MEM_F_TX,
  MEM_F_TXP,
  MEM_GAIN,
  MEM_LAST_F,
  MEM_MODULATION,
  MEM_NAME,
  MEM_P_CAL_H,
  MEM_P_CAL_L,
  MEM_P_CAL_M,
  MEM_PPM,
  MEM_RADIO,
  MEM_READONLY,
  MEM_RX_CODE,
  MEM_RX_CODE_TYPE,
  MEM_SCRAMBLER,
  MEM_SQ,
  MEM_SQ_TYPE,
  MEM_STEP,
  MEM_TX,
  MEM_TX_CODE,
  MEM_TX_CODE_TYPE,
  MEM_TX_OFFSET_DIR,

  MEM_COUNT,
} MemProp;

static bool save(const MenuItem *item, KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && key == KEY_MENU) {
    if (gChNum >= 0) {
      CHANNELS_Save(gChNum, &gChEd);
      APPS_exit();
    } else {
      gChSaveMode = true;
      gChListFilter = TYPE_FILTER_CH_SAVE;
      APPS_run(APP_CH_LIST);
    }
    return true;
  }
  return false;
}

static void PrintRTXCode(char *Output, uint8_t codeType, uint8_t code) {
  if (codeType == CODE_TYPE_CONTINUOUS_TONE) {
    sprintf(Output, "CT:%u.%uHz", CTCSS_Options[code] / 10,
            CTCSS_Options[code] % 10);
  } else if (codeType == CODE_TYPE_DIGITAL) {
    sprintf(Output, "DCS:D%03oN", DCS_Options[code]);
  } else if (codeType == CODE_TYPE_REVERSE_DIGITAL) {
    sprintf(Output, "DCS:D%03oI", DCS_Options[code]);
  } else {
    sprintf(Output, "No code");
  }
}

static uint32_t getValue(MemProp p) {
  switch (p) {
  case MEM_COUNT:
    return MEM_COUNT;
  case MEM_BW:
    return gChEd.bw;
  case MEM_RX_CODE_TYPE:
    return gChEd.code.rx.type;
  case MEM_RX_CODE:
    return gChEd.code.rx.value;
  case MEM_TX_CODE_TYPE:
    return gChEd.code.tx.type;
  case MEM_TX_CODE:
    return gChEd.code.tx.value;
  case MEM_F_TXP:
    return gChEd.power;
  case MEM_TX_OFFSET_DIR:
    return gChEd.offsetDir;
  case MEM_MODULATION:
    return gChEd.modulation;
  case MEM_STEP:
    return gChEd.step;
  case MEM_SQ_TYPE:
    return gChEd.squelch.type;
  case MEM_SQ:
    return gChEd.squelch.value;
  case MEM_PPM:
    return gChEd.ppm + 15;
  case MEM_GAIN:
    return gChEd.gainIndex;
  case MEM_SCRAMBLER:
    return gChEd.scrambler;
  case MEM_TX:
    return gChEd.allowTx;
  case MEM_READONLY:
    return gChEd.meta.readonly;
  case MEM_TYPE:
    return gChEd.meta.type;
  case MEM_BANK:
    return gChEd.misc.bank;
  case MEM_P_CAL_L:
    return gChEd.misc.powCalib.s;
  case MEM_P_CAL_M:
    return gChEd.misc.powCalib.m;
  case MEM_P_CAL_H:
    return gChEd.misc.powCalib.e;
  case MEM_RADIO:
    return gChEd.radio;
  case MEM_START:
  case MEM_F_RX:
    return gChEd.rxF;
  case MEM_END:
  case MEM_F_TX:
    return gChEd.txF;
  case MEM_TX_OFFSET:
    if (gChEd.offsetDir == OFFSET_NONE) {
      return 0;
    } else if (gChEd.offsetDir == OFFSET_MINUS) {
      return -gChEd.txF;
    }
    return gChEd.txF;
  case MEM_LAST_F:
    return gChEd.misc.lastUsedFreq;
  case MEM_BOUNDS:
  case MEM_NAME:
    return 0;
  }
  return 0;
}

static void getValS(const MenuItem *item, char *buf, uint8_t _) {
  const uint32_t v = getValue(item->setting);
  switch (item->setting) {
  case MEM_BOUNDS: {
    const uint32_t fs = gChEd.rxF;
    const uint32_t fe = gChEd.txF;
    sprintf(buf, "%lu.%05lu - %lu.%05lu", fs / MHZ, fs % MHZ, fe / MHZ,
            fe % MHZ);
  } break;
  /* case MEM_START:
    sprintf(buf, "%lu.%03lu", fs / MHZ, fs / 100 % 1000);
    break;
  case MEM_END:
    sprintf(buf, "%lu.%03lu", fe / MHZ, fe / 100 % 1000);
    break; */
  case MEM_NAME:
    strncpy(buf, gChEd.name, 31);
    break;
  case MEM_BW:
    if (gChEd.radio == RADIO_BK4819) {
      strncpy(buf, BW_NAMES_BK4819[gChEd.bw], 31);
    } else if (gChEd.radio == RADIO_SI4732) {
      strncpy(buf,
              ((gChEd.modulation == MOD_LSB || gChEd.modulation == MOD_USB)
                   ? BW_NAMES_SI47XX_SSB
                   : BW_NAMES_SI47XX)[gChEd.bw],
              31);
    } else {
      strncpy(buf, "WFM", 31);
    }
    break;
  case MEM_SQ_TYPE:
    strncpy(buf, SQ_TYPE_NAMES[gChEd.squelch.type], 31);
    break;
  case MEM_PPM:
    sprintf(buf, "%+d", gChEd.ppm);
    break;
  case MEM_SQ:
  case MEM_SCRAMBLER:
  case MEM_BANK:
  case MEM_P_CAL_L:
  case MEM_P_CAL_M:
  case MEM_P_CAL_H:
    sprintf(buf, "%u", v);
    break;
  case MEM_GAIN:
    sprintf(buf, "%ddB", -GAIN_TABLE[gChEd.gainIndex].gainDb + 33);
    break;
  case MEM_MODULATION:
    strncpy(buf, MOD_NAMES_BK4819[gChEd.modulation], 31);
    break;
  case MEM_STEP:
    sprintf(buf, "%u.%02uKHz", StepFrequencyTable[gChEd.step] / 100,
            StepFrequencyTable[gChEd.step] % 100);
    break;
  case MEM_TX:
    strncpy(buf, YES_NO[gChEd.allowTx], 31);
    break;
  case MEM_F_RX:
  case MEM_F_TX:
  case MEM_LAST_F:
  case MEM_TX_OFFSET:
    mhzToS(buf, v);
    break;
  case MEM_RX_CODE_TYPE:
    strncpy(buf, TX_CODE_TYPES[gChEd.code.rx.type], 31);
    break;
  case MEM_RX_CODE:
    PrintRTXCode(buf, gChEd.code.rx.type, gChEd.code.rx.value);
    break;
  case MEM_TX_CODE_TYPE:
    strncpy(buf, TX_CODE_TYPES[gChEd.code.tx.type], 31);
    break;
  case MEM_TX_CODE:
    PrintRTXCode(buf, gChEd.code.tx.type, gChEd.code.tx.value);
    break;
  case MEM_TX_OFFSET_DIR:
    snprintf(buf, 15, TX_OFFSET_NAMES[gChEd.offsetDir]);
    break;
  case MEM_F_TXP:
    snprintf(buf, 15, TX_POWER_NAMES[gChEd.power]);
    break;
  case MEM_READONLY:
    snprintf(buf, 15, YES_NO[gChEd.meta.readonly]);
    break;
  case MEM_TYPE:
    snprintf(buf, 15, CH_TYPE_NAMES[gChEd.meta.type]);
    break;
  case MEM_RADIO:
    snprintf(buf, 15, RADIO_NAMES[gChEd.radio]);
    break;
  }
}

static void setValue(MemProp p, uint32_t v) {
  switch (p) {
  case MEM_BW:
    gChEd.bw = v;
    break;
  case MEM_F_TXP:
    gChEd.power = v;
    break;
  case MEM_TX_OFFSET_DIR:
    gChEd.offsetDir = v;
    break;
  case MEM_MODULATION:
    gChEd.modulation = v;
    break;
  case MEM_STEP:
    gChEd.step = v;
    if (gChEd.meta.type == TYPE_VFO) {
      gChEd.fixedBoundsMode = false;
    }
    break;
  case MEM_SQ_TYPE:
    gChEd.squelch.type = v;
    break;
  case MEM_SQ:
    gChEd.squelch.value = v;
    break;
  case MEM_PPM:
    gChEd.ppm = v - 15;
    break;
  case MEM_GAIN:
    gChEd.gainIndex = v;
    break;
  case MEM_TX:
    gChEd.allowTx = v;
    break;
  case MEM_RX_CODE_TYPE:
    gChEd.code.rx.type = v;
    gChEd.code.rx.value = 0;
    break;
  case MEM_RX_CODE:
    gChEd.code.rx.value = v;
    break;
  case MEM_TX_CODE_TYPE:
    gChEd.code.tx.type = v;
    gChEd.code.rx.value = 0;
    break;
  case MEM_TX_CODE:
    gChEd.code.tx.value = v;
    break;
  case MEM_SCRAMBLER:
    gChEd.scrambler = v;
    break;
  case MEM_READONLY:
    gChEd.meta.readonly = v;
    break;
  case MEM_TYPE:
    gChEd.meta.type = v;
    break;
  case MEM_BANK:
    gChEd.misc.bank = v;
    break;
  case MEM_P_CAL_L:
    gChEd.misc.powCalib.s = v;
    break;
  case MEM_P_CAL_M:
    gChEd.misc.powCalib.m = v;
    break;
  case MEM_P_CAL_H:
    gChEd.misc.powCalib.e = v;
    break;
  case MEM_RADIO:
    gChEd.radio = v;
    break;
  default:
    break;
  }
}

static void updVal(const MenuItem *item, bool inc);

static MenuItem pCalMenuItems[] = {
    {"L", MEM_P_CAL_L, getValS, updVal},
    {"M", MEM_P_CAL_M, getValS, updVal},
    {"H", MEM_P_CAL_H, getValS, updVal},
};

static Menu pCalMenu = {"P cal", pCalMenuItems, ARRAY_SIZE(pCalMenuItems)};

static MenuItem radioMenuItems[] = {
    {"Step", MEM_STEP, getValS, updVal},
    {"Mod", MEM_MODULATION, getValS, updVal},
    {"BW", MEM_BW, getValS, updVal},
    {"Gain", MEM_GAIN, getValS, updVal},
    {"Radio", MEM_RADIO, getValS, updVal},
    {"SQ type", MEM_SQ_TYPE, getValS, updVal},
    {"SQ level", MEM_SQ, getValS, updVal},
    {"RX code type", MEM_RX_CODE_TYPE, getValS, updVal},
    {"RX code", MEM_RX_CODE, getValS, updVal},
    {"TX code type", MEM_TX_CODE_TYPE, getValS, updVal},
    {"TX code", MEM_TX_CODE, getValS, updVal},
    {"TX power", MEM_F_TXP, getValS, updVal},
    {"Scrambler", MEM_SCRAMBLER, getValS, updVal},
    {"Enable TX", MEM_TX, getValS, updVal},
};

static Menu radioMenu = {"Radio", radioMenuItems, ARRAY_SIZE(radioMenuItems)};

// TODO: code type change by #

static MenuItem menuChVfo[] = {
    {"Type", MEM_TYPE, getValS, updVal},
    {"Name", MEM_NAME, getValS, updVal},

    {"RX f", MEM_F_RX, getValS, updVal},
    {"TX f / offset", MEM_F_TX, getValS, updVal},
    {"TX offset dir", MEM_TX_OFFSET_DIR, getValS, updVal},

    {"Radio", .submenu = &radioMenu},

    {"Readonly", MEM_READONLY, getValS, updVal},
    {"Save CH", .action = save},
};

static void applyBounds(uint32_t fs, uint32_t fe) {
  gChEd.rxF = fs;
  gChEd.txF = fe;
}

static bool setBounds(const MenuItem *item, KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && key == KEY_MENU) {
    FINPUT_setup(0, BK4819_F_MAX, UNIT_MHZ, true);
    gFInputCallback = applyBounds;
    APPS_run(APP_FINPUT);
    return true;
  }
  return false;
}

static MenuItem menuBand[] = {
    {"Type", MEM_TYPE, getValS, updVal},
    {"Name", MEM_NAME, getValS, updVal},

    (MenuItem){"Bounds", MEM_BOUNDS, getValS, .action = setBounds},

    {"Radio", .submenu = &radioMenu},

    {"P cal", .submenu = &pCalMenu},
    {"Last f", MEM_LAST_F, getValS, updVal},
    {"PPM", MEM_PPM, getValS, updVal},

    {"Bank", MEM_BANK, getValS, updVal},
    {"Readonly", MEM_READONLY, getValS, updVal},
    {"Save BAND", .action = save},
};

static Menu *menu;

static Menu chMenu = {"CH ed", menuChVfo, ARRAY_SIZE(menuChVfo)};
static Menu bandMenu = {"CH ed", menuBand, ARRAY_SIZE(menuBand)};

static void updVal(const MenuItem *item, bool inc) {
  uint32_t v = getValue(item->setting);
  switch (item->setting) {
  case MEM_TYPE:
    v = IncDecU(v, 0, TYPE_SETTING, inc);
    setValue(item->setting, v);
    menu = v == TYPE_BAND ? &bandMenu : &chMenu;
    MENU_Init(menu);
    break;
  case MEM_START:
    break;
  case MEM_END:
    break;
  case MEM_NAME:
    break;
  case MEM_BW:
    break;
  case MEM_SQ_TYPE:
    break;
  case MEM_PPM:
    break;
  case MEM_SQ:
    break;
  case MEM_SCRAMBLER:
    break;
  case MEM_BANK:
    break;
  case MEM_P_CAL_L:
    break;
  case MEM_P_CAL_M:
    break;
  case MEM_P_CAL_H:
    break;
  case MEM_GAIN:
    break;
  case MEM_MODULATION:
    break;
  case MEM_STEP:
    break;
  case MEM_TX:
    break;
  case MEM_F_RX:
    break;
  case MEM_F_TX:
    break;
  case MEM_LAST_F:
    break;
  case MEM_RX_CODE_TYPE:
    break;
  case MEM_RX_CODE:
    break;
  case MEM_TX_CODE_TYPE:
    break;
  case MEM_TX_CODE:
    break;
  case MEM_TX_OFFSET:
    break;
  case MEM_TX_OFFSET_DIR:
    break;
  case MEM_F_TXP:
    break;
  case MEM_READONLY:
    break;
  case MEM_RADIO:
    break;
  default:
    break;
  }
}

static void setPCalL(uint32_t f) {
  gChEd.misc.powCalib.s = Clamp(f / MHZ, 0, 255);
}

static void setPCalM(uint32_t f) {
  gChEd.misc.powCalib.m = Clamp(f / MHZ, 0, 255);
}

static void setPCalH(uint32_t f) {
  gChEd.misc.powCalib.e = Clamp(f / MHZ, 0, 255);
}

static void setRXF(uint32_t f) { gChEd.rxF = f; }

static void setTXF(uint32_t f) { gChEd.txF = f; }

static void setLastF(uint32_t f) { gChEd.misc.lastUsedFreq = f; }

/* static void updateTxCodeListSize() {
  for (uint8_t i = 0; i < menuSize; ++i) {
    MenuItem *item = &menu[i];
    uint8_t type = CODE_TYPE_OFF;
    bool isCodeTypeMenu = false;

    if (item->type == M_TX_CODE) {
      type = gChEd.code.tx.type;
      isCodeTypeMenu = true;
    } else if (item->type == M_RX_CODE) {
      type = gChEd.code.rx.type;
      isCodeTypeMenu = true;
    }
    if (type == CODE_TYPE_CONTINUOUS_TONE) {
      item->size = ARRAY_SIZE(CTCSS_Options);
    } else if (type != CODE_TYPE_OFF) {
      item->size = ARRAY_SIZE(DCS_Options);
    } else if (isCodeTypeMenu) {
      item->size = 1;
    }
  }
} */

void CHCFG_init(void) {
  // updateTxCodeListSize();

  if (gChEd.meta.type == TYPE_BAND) {
    gChEd.meta.type = TYPE_BAND;
    menu = &bandMenu;
  } else {
    gChEd.meta.type = TYPE_CH;
    menu = &chMenu;
  }

  MENU_Init(menu);
}

void CHCFG_deinit(void) { gChNum = -1; }

bool CHCFG_key(KEY_Code_t key, Key_State_t state) {
  if (MENU_HandleInput(key, state)) {
    return true;
  }
  return false;
}

void CHCFG_render(void) { MENU_Render(); }
