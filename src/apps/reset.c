#include "reset.h"
#include "../driver/eeprom.h"
#include "../driver/st7565.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../helper/channels.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"

typedef enum {
  RESET_0xFF,
  RESET_FULL,
  RESET_CHANNELS,
  RESET_BANDS,
  RESET_UNKNOWN,
} ResetType;

static char *RESET_TYPE_NAMES[] = {"0xFF", "FULL", "CHANNELS", "BANDS"};

static struct {
  uint32_t totalBytes;
  uint32_t doneBytes;
  uint16_t pageSize;
  uint16_t maxChannels;
  uint16_t currentItem;
  ResetType type;
} resetState;

static VFO defaultVfos[9] = {
    {.rxF = 14550000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 17230000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 25355000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 40065000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 43392500,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 43780000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 86800000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
    {.rxF = 25220000,
     .meta.type = TYPE_CH,
     .gainIndex = ARRAY_SIZE(GAIN_TABLE) - 1,
     .radio = RADIO_BK4819,
     .name = "Test CH"},
    {.rxF = 10440000,
     .meta.type = TYPE_VFO,
     .gainIndex = AUTO_GAIN_INDEX,
     .radio = RADIO_BK4819},
};

static void startReset(ResetType type) {
  resetState.type = type;
  resetState.doneBytes = 0;
  resetState.currentItem = 0;

  if (type == RESET_0xFF) {
    resetState.totalBytes = SETTINGS_GetEEPROMSize();
  } else {
    uint16_t items = (type == RESET_FULL) ? (1 + 9 + resetState.maxChannels - 9)
                     : (type == RESET_CHANNELS) ? (resetState.maxChannels - 9)
                                                : 0;
    resetState.totalBytes = items * CH_SIZE;
  }
}

static bool processReset(void) {
  if (resetState.type == RESET_0xFF) {
    uint16_t page = resetState.doneBytes / resetState.pageSize;
    EEPROM_ClearPage(page);
    resetState.doneBytes += resetState.pageSize;
    return resetState.doneBytes >= resetState.totalBytes;
  }

  if (resetState.type == RESET_FULL && resetState.currentItem == 0) {
    SETTINGS_Save();
    resetState.doneBytes += SETTINGS_SIZE;
    resetState.currentItem++;
    return false;
  }

  if (resetState.type == RESET_FULL && resetState.currentItem < 10) {
    VFO vfo = defaultVfos[resetState.currentItem - 1];
    if (vfo.meta.type == TYPE_VFO) {
      sprintf(vfo.name, "VFO-%c", 'A' + resetState.currentItem - 1);
    }
    vfo.channel = 0;
    vfo.modulation = MOD_FM;
    vfo.bw = BK4819_FILTER_BW_12k;
    vfo.txF = 0;
    vfo.offsetDir = OFFSET_NONE;
    vfo.allowTx = false;
    vfo.code.rx.type = 0;
    vfo.code.tx.type = 0;
    vfo.meta.readonly = false;
    vfo.squelch.value = 4;
    vfo.step = STEP_25_0kHz;
    CHANNELS_Save(resetState.maxChannels - 9 + resetState.currentItem - 1,
                  &vfo);
    resetState.doneBytes += CH_SIZE;
    resetState.currentItem++;
    return false;
  }

  if (resetState.currentItem < resetState.maxChannels - 9) {
    CHANNELS_Delete(resetState.currentItem -
                    (resetState.type == RESET_FULL ? 10 : 0));
    resetState.doneBytes += CH_SIZE;
    resetState.currentItem++;
    return false;
  }

  return true;
}

void RESET_Init(void) {
  resetState.type = RESET_UNKNOWN;
  gSettings.keylock = false;
  gSettings.eepromType = EEPROM_DetectType();
  resetState.pageSize = SETTINGS_GetPageSize();
  resetState.maxChannels = CHANNELS_GetCountMax();
}

void RESET_Update(void) {
  if (gSettings.eepromType == EEPROM_UNKNOWN ||
      resetState.type == RESET_UNKNOWN) {
    return;
  }

  if (!processReset()) {
    gRedrawScreen = true;
    return;
  }

  NVIC_SystemReset();
}

void RESET_Render(void) {
  STATUSLINE_SetText("%s", EEPROM_TYPE_NAMES[gSettings.eepromType]);

  if (resetState.type == RESET_UNKNOWN) {
    for (uint8_t i = 0; i < ARRAY_SIZE(RESET_TYPE_NAMES); i++) {
      PrintMedium(2, 18 + i * 8, "%u: %s", i, RESET_TYPE_NAMES[i]);
    }
    return;
  }

  STATUSLINE_SetText("%s: %s", EEPROM_TYPE_NAMES[gSettings.eepromType],
                     RESET_TYPE_NAMES[resetState.type]);

  uint8_t progress =
      ConvertDomain(resetState.doneBytes, 0, resetState.totalBytes, 0, 100);
  const uint8_t TOP = 28;

  DrawRect(13, TOP, 102, 9, C_FILL);
  FillRect(14, TOP + 1, progress, 7, C_FILL);
  PrintMediumEx(LCD_XCENTER, TOP + 7, POS_C, C_INVERT, "%u%", progress);
}

bool RESET_key(KEY_Code_t k, Key_State_t state) {
  if (state == KEY_RELEASED && resetState.type == RESET_UNKNOWN) {
    uint8_t t = k - KEY_0;
    if (t < ARRAY_SIZE(RESET_TYPE_NAMES)) {
      startReset(t);
      return true;
    }
  }
  return false;
}
