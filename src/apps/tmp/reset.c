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
  RESET_UNKNOWN,
} ResetType;

static char *RESET_TYPE_NAMES[] = {"0xFF", "FULL"};

static struct {
  uint32_t totalBytes;
  uint32_t doneBytes;
  uint16_t pageSize;
  uint16_t maxChannels;
  uint16_t currentItem;
  ResetType type;
} resetState;

static VFO defaultVfos[4] = {
    {.rxF = 14550000},
    {.rxF = 43312500},
    {.rxF = 25355000},
    {.rxF = 40065000},
};

static void startReset(ResetType type) {
  resetState.type = type;
  resetState.doneBytes = 0;
  resetState.currentItem = 0;
  resetState.totalBytes = (type == RESET_0xFF)
                              ? SETTINGS_GetEEPROMSize()
                              : resetState.maxChannels * CH_SIZE;
}

static bool processReset(void) {
  if (resetState.type == RESET_0xFF) {
    uint16_t page = resetState.doneBytes / resetState.pageSize;
    EEPROM_ClearPage(page);
    resetState.doneBytes += resetState.pageSize;
    return resetState.doneBytes >= resetState.totalBytes;
  }

  // RESET_FULL
  if (resetState.currentItem == 0) {
    SETTINGS_Save();
    resetState.doneBytes += SETTINGS_SIZE;
    resetState.currentItem++;
    return false;
  }

  uint16_t chIndex = resetState.currentItem - 1;
  uint8_t numVFOs = ARRAY_SIZE(defaultVfos);

  if (chIndex < resetState.maxChannels - numVFOs) {
    CHANNELS_Delete(chIndex);
    resetState.doneBytes += CH_SIZE;
    resetState.currentItem++;
    return false;
  }

  if (chIndex < resetState.maxChannels) {
    uint8_t vfoIndex = chIndex - (resetState.maxChannels - numVFOs);
    VFO vfo = defaultVfos[vfoIndex];
    sprintf(vfo.name, "VFO-%c", 'A' + vfoIndex);
    vfo.meta.type = TYPE_VFO;
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
    vfo.gainIndex = AUTO_GAIN_INDEX;
    vfo.radio = RADIO_BK4819;
    CHANNELS_Save(chIndex, &vfo);
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
  STATUSLINE_SetText("%s", EEPROM_TYPE_NAMES[gSettings.eepromType]);
}

void RESET_Update(void) {
  if (resetState.type == RESET_UNKNOWN || !processReset()) {
    return;
  }
  NVIC_SystemReset();
}

void RESET_Render(void) {
  if (resetState.type == RESET_UNKNOWN) {
    for (uint8_t i = 0; i < ARRAY_SIZE(RESET_TYPE_NAMES); i++) {
      PrintMedium(2, 18 + i * 8, "%u: %s", i, RESET_TYPE_NAMES[i]);
    }
    return;
  }

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
