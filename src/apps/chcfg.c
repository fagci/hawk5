#include "chcfg.h"
#include "../dcs.h"
#include "../driver/st7565.h"
#include "../driver/uart.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../misc.h"
#include "../radio.h"
#include "../ui/graphics.h"
#include "../ui/menu.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chlist.h"
#include "finput.h"
#include "textinput.h"

static uint8_t menuIndex = 0;
static uint8_t subMenuIndex = 0;
static bool isSubMenu = false;

CH gChEd;
int16_t gChNum = -1;

static MenuItem *menu;
static MenuItem menuChVfo[] = {
    {"Type", M_TYPE, ARRAY_SIZE(CH_TYPE_NAMES)},
    {"Name", M_NAME, 0},
    {"Step", M_STEP, ARRAY_SIZE(StepFrequencyTable)},
    {"Modulation", M_MODULATION, ARRAY_SIZE(modulationTypeOptions)},
    {"BW", M_BW, 10},
    {"Gain", M_GAIN, ARRAY_SIZE(gainTable)},
    {"Radio", M_RADIO, 2},
    {"SQ type", M_SQ_TYPE, ARRAY_SIZE(sqTypeNames)},
    {"SQ level", M_SQ, 10},
    {"RX f", M_F_RX, 0},
    {"TX f / offset", M_F_TX, 0},
    {"TX offset dir", M_TX_OFFSET_DIR, ARRAY_SIZE(TX_OFFSET_NAMES)},
    {"RX code type", M_RX_CODE_TYPE, ARRAY_SIZE(TX_CODE_TYPES)},
    {"RX code", M_RX_CODE, 0},
    {"TX code type", M_TX_CODE_TYPE, ARRAY_SIZE(TX_CODE_TYPES)},
    {"TX code", M_TX_CODE, 0},
    {"TX power", M_F_TXP, ARRAY_SIZE(TX_POWER_NAMES)},
    {"Scrambler", M_SCRAMBLER, 16},
    {"Enable TX", M_TX, 2},
    {"Readonly", M_READONLY, 2},
    {"Save CH", M_SAVE, 0},
};

static MenuItem menuBand[] = {
    {"Type", M_TYPE, ARRAY_SIZE(CH_TYPE_NAMES)},
    {"Name", M_NAME, 0},
    {"Step", M_STEP, ARRAY_SIZE(StepFrequencyTable)},
    {"Modulation", M_MODULATION, ARRAY_SIZE(modulationTypeOptions)},
    {"BW", M_BW, 10},
    {"Gain", M_GAIN, ARRAY_SIZE(gainTable)},
    {"Radio", M_RADIO, 2},
    {"SQ type", M_SQ_TYPE, ARRAY_SIZE(sqTypeNames)},
    {"SQ level", M_SQ, 10},
    {"RX f", M_F_RX, 0},
    {"TX f / offset", M_F_TX, 0},
    {"TX offset dir", M_TX_OFFSET_DIR, ARRAY_SIZE(TX_OFFSET_NAMES)},

    {"Bank", M_BANK, 128},
    {"P cal L", M_P_CAL_L, 0},
    {"P cal M", M_P_CAL_M, 0},
    {"P cal H", M_P_CAL_H, 0},
    {"Last f", M_LAST_F, 0},
    {"ppm", M_PPM, 31},

    {"TX power", M_F_TXP, ARRAY_SIZE(TX_POWER_NAMES)},
    {"Scrambler", M_SCRAMBLER, 16},
    {"Enable TX", M_TX, 2},
    {"Readonly", M_READONLY, 2},
    {"Save BAND", M_SAVE, 0},
};
static uint8_t menuSize = 0;

static void apply() {
  switch (gChEd.meta.type) {
  case TYPE_VFO:
    radio = gChEd;
    RADIO_SetupByCurrentVFO();
    RADIO_SaveCurrentVFO();
    break;
  case TYPE_CH:
    break;
  default:
    break;
  }
}

static void setPCalL(uint32_t f) {
  gChEd.misc.powCalib.s = Clamp(f / MHZ, 0, 255);
  apply();
}

static void setPCalM(uint32_t f) {
  gChEd.misc.powCalib.m = Clamp(f / MHZ, 0, 255);
  apply();
}

static void setPCalH(uint32_t f) {
  gChEd.misc.powCalib.e = Clamp(f / MHZ, 0, 255);
  apply();
}

static void setRXF(uint32_t f) {
  gChEd.rxF = f;
  apply();
}

static void setTXF(uint32_t f) {
  gChEd.txF = f;
  apply();
}

static void setLastF(uint32_t f) {
  gChEd.misc.lastUsedFreq = f;
  apply();
}

static void getMenuItemValue(BandCfgMenu type, char *Output) {
  uint32_t fs = gChEd.rxF;
  uint32_t fe = gChEd.txF;
  switch (type) {
  case M_START:
    sprintf(Output, "%lu.%03lu", fs / MHZ, fs / 100 % 1000);
    break;
  case M_END:
    sprintf(Output, "%lu.%03lu", fe / MHZ, fe / 100 % 1000);
    break;
  case M_NAME:
    strncpy(Output, gChEd.name, 31);
    break;
  case M_BW:
    strncpy(Output, RADIO_GetBWName(), 31);
    break;
  case M_SQ_TYPE:
    strncpy(Output, sqTypeNames[gChEd.squelch.type], 31);
    break;
  case M_PPM:
    sprintf(Output, "%+d", gChEd.ppm);
    break;
  case M_SQ:
    sprintf(Output, "%u", gChEd.squelch.value);
    break;
  case M_SCRAMBLER:
    sprintf(Output, "%u", gChEd.scrambler);
    break;
  case M_BANK:
    sprintf(Output, "%u", gChEd.misc.bank);
    break;
  case M_P_CAL_L:
    sprintf(Output, "%u", gChEd.misc.powCalib.s);
    break;
  case M_P_CAL_M:
    sprintf(Output, "%u", gChEd.misc.powCalib.m);
    break;
  case M_P_CAL_H:
    sprintf(Output, "%u", gChEd.misc.powCalib.e);
    break;
  case M_GAIN:
    sprintf(Output, "%ddB", -gainTable[gChEd.gainIndex].gainDb + 33);
    break;
  case M_MODULATION:
    strncpy(Output, modulationTypeOptions[gChEd.modulation], 31);
    break;
  case M_STEP:
    sprintf(Output, "%u.%02uKHz", StepFrequencyTable[gChEd.step] / 100,
            StepFrequencyTable[gChEd.step] % 100);
    break;
  case M_TX:
    strncpy(Output, yesNo[gChEd.allowTx], 31);
    break;
  case M_F_RX:
    sprintf(Output, "%u.%05u", gChEd.rxF / MHZ, gChEd.rxF % MHZ);
    break;
  case M_F_TX:
    sprintf(Output, "%u.%05u", gChEd.txF / MHZ, gChEd.txF % MHZ);
    break;
  case M_LAST_F:
    sprintf(Output, "%u.%05u", gChEd.misc.lastUsedFreq / MHZ,
            gChEd.misc.lastUsedFreq % MHZ);
    break;
  case M_RX_CODE_TYPE:
    strncpy(Output, TX_CODE_TYPES[gChEd.code.rx.type], 31);
    break;
  case M_RX_CODE:
    PrintRTXCode(Output, gChEd.code.rx.type, gChEd.code.rx.value);
    break;
  case M_TX_CODE_TYPE:
    strncpy(Output, TX_CODE_TYPES[gChEd.code.tx.type], 31);
    break;
  case M_TX_CODE:
    PrintRTXCode(Output, gChEd.code.tx.type, gChEd.code.tx.value);
    break;
  case M_TX_OFFSET:
    sprintf(Output, "%u.%05u", gChEd.txF / MHZ, gChEd.txF % MHZ);
    break;
  case M_TX_OFFSET_DIR:
    snprintf(Output, 15, TX_OFFSET_NAMES[gChEd.offsetDir]);
    break;
  case M_F_TXP:
    snprintf(Output, 15, TX_POWER_NAMES[gChEd.power]);
    break;
  case M_READONLY:
    snprintf(Output, 15, yesNo[gChEd.meta.readonly]);
    break;
  case M_TYPE:
    snprintf(Output, 15, CH_TYPE_NAMES[gChEd.meta.type]);
    break;
  case M_RADIO:
    snprintf(Output, 15, radioNames[gChEd.radio]);
    break;
  default:
    break;
  }
}

static void acceptRadioConfig(const MenuItem *item, uint8_t subMenuIndex) {
  switch (item->type) {
  case M_BW:
    gChEd.bw = subMenuIndex;
    break;
  case M_F_TXP:
    gChEd.power = subMenuIndex;
    break;
  case M_TX_OFFSET_DIR:
    gChEd.offsetDir = subMenuIndex;
    break;
  case M_MODULATION:
    gChEd.modulation = subMenuIndex;
    break;
  case M_STEP:
    gChEd.step = subMenuIndex;
    if (gChEd.meta.type == TYPE_VFO) {
      gChEd.fixedBoundsMode = false;
    }
    break;
  case M_SQ_TYPE:
    gChEd.squelch.type = subMenuIndex;
    break;
  case M_SQ:
    gChEd.squelch.value = subMenuIndex;
    break;
  case M_PPM:
    gChEd.ppm = subMenuIndex - 15;
    break;
  case M_GAIN:
    gChEd.gainIndex = subMenuIndex;
    break;
  case M_TX:
    gChEd.allowTx = subMenuIndex;
    break;
  case M_RX_CODE_TYPE:
    gChEd.code.rx.type = subMenuIndex;
    gChEd.code.rx.value = 0;
    break;
  case M_RX_CODE:
    gChEd.code.rx.value = subMenuIndex;
    break;
  case M_TX_CODE_TYPE:
    gChEd.code.tx.type = subMenuIndex;
    gChEd.code.rx.value = 0;
    break;
  case M_TX_CODE:
    gChEd.code.tx.value = subMenuIndex;
    break;
  case M_SCRAMBLER:
    gChEd.scrambler = subMenuIndex;
    break;
  case M_READONLY:
    gChEd.meta.readonly = subMenuIndex;
    break;
  case M_TYPE:
    gChEd.meta.type = subMenuIndex;
    break;
  case M_BANK:
    gChEd.misc.bank = subMenuIndex;
    break;
  /* case M_P_CAL_L:
    gChEd.misc.powCalib.s = subMenuIndex;
    break;
  case M_P_CAL_M:
    gChEd.misc.powCalib.m = subMenuIndex;
    break;
  case M_P_CAL_H:
    gChEd.misc.powCalib.e = subMenuIndex;
    break; */
  case M_RADIO:
    if (RADIO_HasSi() && subMenuIndex > 0) {
      gChEd.radio = RADIO_SI4732;
    } else {
      gChEd.radio = subMenuIndex;
    }
    break;
  default:
    break;
  }

  apply();
}

static void setInitialSubmenuIndex(void) {
  const MenuItem *item = &menu[menuIndex];
  switch (item->type) {
  case M_BW:
    subMenuIndex = gChEd.bw;
    break;
  case M_RX_CODE_TYPE:
    subMenuIndex = gChEd.code.rx.type;
    break;
  case M_RX_CODE:
    subMenuIndex = gChEd.code.rx.value;
    break;
  case M_TX_CODE_TYPE:
    subMenuIndex = gChEd.code.tx.type;
    break;
  case M_TX_CODE:
    subMenuIndex = gChEd.code.tx.value;
    break;
  case M_F_TXP:
    subMenuIndex = gChEd.power;
    break;
  case M_TX_OFFSET_DIR:
    subMenuIndex = gChEd.offsetDir;
    break;
  case M_MODULATION:
    subMenuIndex = gChEd.modulation;
    break;
  case M_STEP:
    subMenuIndex = gChEd.step;
    break;
  case M_SQ_TYPE:
    subMenuIndex = gChEd.squelch.type;
    break;
  case M_SQ:
    subMenuIndex = gChEd.squelch.value;
    break;
  case M_PPM:
    subMenuIndex = gChEd.ppm + 15;
    break;
  case M_GAIN:
    subMenuIndex = gChEd.gainIndex;
    break;
  case M_SCRAMBLER:
    subMenuIndex = gChEd.scrambler;
    break;
  case M_TX:
    subMenuIndex = gChEd.allowTx;
    break;
  case M_READONLY:
    subMenuIndex = gChEd.meta.readonly;
    break;
  case M_TYPE:
    subMenuIndex = gChEd.meta.type;
    break;
  case M_BANK:
    subMenuIndex = gChEd.misc.bank;
    break;
  /* case M_P_CAL_L:
    subMenuIndex = gChEd.misc.powCalib.s;
    break;
  case M_P_CAL_M:
    subMenuIndex = gChEd.misc.powCalib.m;
    break;
  case M_P_CAL_H:
    subMenuIndex = gChEd.misc.powCalib.e;
    break; */
  case M_RADIO:
    if (RADIO_HasSi() && gChEd.radio > 0) {
      subMenuIndex = RADIO_SI4732;
    } else {
      subMenuIndex = gChEd.radio;
    }
    break;
  default:
    subMenuIndex = 0;
    break;
  }
}

static void getMenuItemText(uint16_t index, char *name) {
  MenuItem *m = &menu[index];
  const char *text = m->name;

  if (gChEd.meta.type == TYPE_BAND) {
    switch (m->type) {
    case M_F_RX:
      text = "Start f";
      break;
    case M_F_TX:
      text = "End f";
      break;
    default:
      break;
    }
  }

  strncpy(name, text, 31);
  name[31] = '\0';
}

static void updateTxCodeListSize() {
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
}

static void getSubmenuItemText(uint16_t index, char *name) {
  VFO vfo = gChEd;
  uint16_t tmp = 0;
  switch (menu[menuIndex].type) {
  case M_MODULATION:
    strncpy(name, modulationTypeOptions[index], 31);
    return;
  case M_BW:
    tmp = radio.bw;
    radio.bw = index;
    strncpy(name, RADIO_GetBWName(), 15);
    radio.bw = tmp;
    return;
  case M_RX_CODE_TYPE:
    strncpy(name, TX_CODE_TYPES[index], 15);
    return;
  case M_RX_CODE:
    PrintRTXCode(name, radio.code.rx.type, index);
    return;
  case M_TX_CODE_TYPE:
    strncpy(name, TX_CODE_TYPES[index], 15);
    return;
  case M_TX_CODE:
    PrintRTXCode(name, radio.code.tx.type, index);
    return;
  case M_F_TXP:
    strncpy(name, TX_POWER_NAMES[index], 15);
    return;
  case M_TX_OFFSET_DIR:
    strncpy(name, TX_OFFSET_NAMES[index], 15);
    return;
  case M_STEP:
    sprintf(name, "%u.%02uKHz", StepFrequencyTable[index] / 100,
            StepFrequencyTable[index] % 100);
    return;
  case M_SQ_TYPE:
    strncpy(name, sqTypeNames[index], 31);
    return;
  case M_RADIO:
    strncpy(name, radioNames[RADIO_HasSi() && index > 0 ? RADIO_SI4732 : index],
            31);
    return;
  case M_SQ:
  case M_SCRAMBLER:
  case M_BANK:
  case M_P_CAL_L:
  case M_P_CAL_M:
  case M_P_CAL_H:
    sprintf(name, "%u", index);
    return;
  case M_PPM:
    sprintf(name, "%+d", index - 15);
    return;
  case M_GAIN:
    sprintf(name, index == AUTO_GAIN_INDEX ? "auto" : "%+ddB",
            -gainTable[index].gainDb + 33);
    return;
  case M_TX:
  case M_READONLY:
    strncpy(name, yesNo[index], 31);
    return;
  case M_TYPE:
    strncpy(name, CH_TYPE_NAMES[index], 31);
    return;
  default:
    break;
  }
}

void CHCFG_init(void) {
  if (gChEd.meta.type == TYPE_BAND) {
    menu = menuBand;
    menuSize = ARRAY_SIZE(menuBand);
  }
  if (gChEd.meta.type == TYPE_CH || gChEd.meta.type == TYPE_VFO) {
    menu = menuChVfo;
    menuSize = ARRAY_SIZE(menuChVfo);
  }

  updateTxCodeListSize();

  if (menuIndex > menuSize) {
    menuIndex = menuSize - 1;
  }
}

void CHCFG_deinit(void) { gChNum = -1; }

static bool accept(void) {
  const MenuItem *item = &menu[menuIndex];
  // RUN APPS HERE
  switch (item->type) {
  case M_P_CAL_L:
    gFInputCallback = setPCalL;
    gFInputTempFreq = gChEd.misc.powCalib.s * MHZ;
    APPS_run(APP_FINPUT);
    return true;
  case M_P_CAL_M:
    gFInputCallback = setPCalM;
    gFInputTempFreq = gChEd.misc.powCalib.m * MHZ;
    APPS_run(APP_FINPUT);
    return true;
  case M_P_CAL_H:
    gFInputCallback = setPCalH;
    gFInputTempFreq = gChEd.misc.powCalib.e * MHZ;
    APPS_run(APP_FINPUT);
    return true;
  case M_F_RX:
    gFInputCallback = setRXF;
    gFInputTempFreq = gChEd.rxF;
    APPS_run(APP_FINPUT);
    return true;
  case M_F_TX:
    gFInputCallback = setTXF;
    gFInputTempFreq = gChEd.txF;
    APPS_run(APP_FINPUT);
    return true;
  case M_LAST_F:
    gFInputCallback = setLastF;
    gFInputTempFreq = gChEd.misc.lastUsedFreq;
    APPS_run(APP_FINPUT);
    return true;
  case M_NAME:
    gTextinputText = gChEd.name;
    gTextInputSize = 9;
    APPS_run(APP_TEXTINPUT);
    return true;
  case M_SAVE:
    if (gChNum >= 0) { // editing existing channel
      CHANNELS_Save(gChNum, &gChEd);
      APPS_exit();
      return true;
    }
    gChSaveMode = true;
    gChListFilter = (1 << gChEd.meta.type) | (1 << TYPE_EMPTY);
    APPS_run(APP_CH_LIST);
    return true;
  case M_STEP:
    if (gChEd.meta.type == TYPE_VFO) {
      gChEd.fixedBoundsMode = false;
    }
    break;
  default:
    break;
  }
  // updateTxCodeListSize();
  CHCFG_init();
  if (isSubMenu) {
    acceptRadioConfig(item, subMenuIndex);
    isSubMenu = false;
  } else {
    isSubMenu = true;
    setInitialSubmenuIndex();
  }
  return true;
}

static void setMenuIndexAndRun(uint16_t v) {
  if (isSubMenu) {
    subMenuIndex = v - 1;
  } else {
    menuIndex = v - 1;
  }
  accept();
}

static void upDown(bool inc) {
  if (isSubMenu) {
    subMenuIndex = IncDecU(subMenuIndex, 0, menu[menuIndex].size, inc);
    acceptRadioConfig(&menu[menuIndex], subMenuIndex);
  } else {
    menuIndex = IncDecU(menuIndex, 0, menuSize, inc);
  }
}

bool CHCFG_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(menuIndex + 1, 1, menuSize);
      gNumNavCallback = setMenuIndexAndRun;
    }
    if (gIsNumNavInput) {
      uint8_t v = NUMNAV_Input(key) - 1;
      if (isSubMenu) {
        subMenuIndex = v;
      } else {
        menuIndex = v;
      }
      return true;
    }
  }

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      upDown(key != KEY_UP);
      return true;
    case KEY_MENU:
      return accept();
    case KEY_EXIT:
      if (isSubMenu) {
        isSubMenu = false;
      } else {
        APPS_exit();
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

void CHCFG_render(void) {
  if (gIsNumNavInput) {
    STATUSLINE_SetText("Select: %s", gNumNavInput);
  } else {
    STATUSLINE_SetText(gChEd.name);
  }
  MenuItem *item = &menu[menuIndex];
  if (isSubMenu) {
    UI_ShowMenu(getSubmenuItemText, item->size, subMenuIndex);
    STATUSLINE_SetText(item->name);
  } else {
    UI_ShowMenu(getMenuItemText, menuSize, menuIndex);
    char Output[32] = "";
    getMenuItemValue(item->type, Output);
    PrintMediumEx(LCD_XCENTER, LCD_HEIGHT - 4, POS_C, C_FILL, Output);
  }
}
