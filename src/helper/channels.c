#include "channels.h"
#include "../driver/eeprom.h"
#include "../driver/uart.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../radio.h"
#include "../system.h"
#include <stddef.h>
#include <string.h>

uint16_t gScanlistSize = 0;
uint16_t gScanlist[SCANLIST_MAX] = {0};
CHType gScanlistType = TYPE_CH;
const char *CH_TYPE_NAMES[6] = {"EMPTY", "CH",     "BAND",
                                "VFO",   "FOLDER", "MELODY"};
const char *TX_POWER_NAMES[4] = {"ULow", "Low", "Mid", "High"};
const char *TX_OFFSET_NAMES[3] = {"None", "+", "-"};
const char *TX_CODE_TYPES[4] = {"None", "CT", "DCS", "-DCS"};

static uint32_t getChannelsEnd() {
  uint32_t eepromSize = SETTINGS_GetEEPROMSize();
  uint32_t minSizeWithPatch = CHANNELS_OFFSET + CH_SIZE + PATCH_SIZE;
  if (eepromSize < minSizeWithPatch) {
    return eepromSize;
  }
  return eepromSize - PATCH_SIZE;
}

static uint32_t GetChannelOffset(int16_t num) {
  return CHANNELS_OFFSET + num * CH_SIZE;
}

uint16_t CHANNELS_GetCountMax(void) {
  uint16_t n = (getChannelsEnd() - CHANNELS_OFFSET) / CH_SIZE;
  return n < SCANLIST_MAX ? n : SCANLIST_MAX;
}

void CHANNELS_Load(int16_t num, CH *p) {
  if (num >= 0) {
    EEPROM_ReadBuffer(GetChannelOffset(num), p, CH_SIZE);
    Log(">> R CH%u '%s': f=%u, radio=%u, type=%s", num, p->name, p->rxF,
        p->radio, CH_TYPE_NAMES[p->meta.type]);
  }
}

void CHANNELS_Save(int16_t num, CH *p) {
  if (num >= 0) {
    Log(">> W CH%u OFS=%u '%s': f=%u, radio=%u", num, GetChannelOffset(num),
        p->name, p->rxF, p->radio);
    EEPROM_WriteBuffer(GetChannelOffset(num), p, CH_SIZE);
  }
}

void CHANNELS_Delete(int16_t num) {
  CH _ch;
  memset(&_ch, 0, sizeof(_ch));
  CHANNELS_Save(num, &_ch);
}

bool CHANNELS_Existing(int16_t num) {
  if (num < 0 || num >= CHANNELS_GetCountMax()) {
    return false;
  }
  return CHANNELS_GetMeta(num).type != TYPE_EMPTY;
}

uint16_t CHANNELS_Scanlists(int16_t num) {
  uint16_t sl;
  EEPROM_ReadBuffer(GetChannelOffset(num) + offsetof(CH, scanlists), &sl, 2);
  return sl;
}
static int16_t chScanlistIndex = 0;

int16_t CHANNELS_GetCurrentScanlistCH() {
  if (gScanlistSize) {
    return gScanlist[chScanlistIndex];
  }
  return -1;
}

void CHANNELS_Next(bool next) {
  if (gScanlistSize) {
    chScanlistIndex = IncDecI(chScanlistIndex, 0, gScanlistSize, next);
    // CHANNELS_LoadCurrentScanlistCH();
  }
}

void CHANNELS_SetScanlistIndexFromRadio() {
  /* if (RADIO_IsChMode() && gScanlistSize) {
    for (uint16_t i = 0; i < gScanlistSize; ++i) {
      if (gScanlist[i] == radio.channel) {
        chScanlistIndex = i;
        break;
      }
    }
  } */
}

static CHTypeFilter _typeFilter = TYPE_FILTER_BAND_SAVE;
static uint16_t _scanlistMask = UINT16_MAX;
void CHANNELS_LoadScanlist(CHTypeFilter typeFilter, uint16_t scanlistMask) {
  if (_typeFilter == typeFilter && _scanlistMask == scanlistMask) {
    return;
  }
  _typeFilter = typeFilter;
  _scanlistMask = scanlistMask;

  Log("Load SL w type_filter=%u", typeFilter);
  if (gSettings.currentScanlist != scanlistMask) {
    gSettings.currentScanlist = scanlistMask;
    SETTINGS_Save();
  }
  gScanlistSize = 0;
  for (uint16_t i = 0; i < CHANNELS_GetCountMax(); ++i) {
    CHMeta meta = CHANNELS_GetMeta(i);
    bool isSaveFilter = typeFilter == TYPE_FILTER_BAND_SAVE ||
                        typeFilter == TYPE_FILTER_CH_SAVE;
    bool isEmptyChannelToSave = meta.type == TYPE_EMPTY && isSaveFilter;

    bool isOurType =
        (typeFilter & (1 << meta.type)) != 0 || isEmptyChannelToSave;
    if (!isOurType) {
      continue;
    }

    bool isOurScanlist = (isOurType && scanlistMask == SCANLIST_ALL) ||
                         (CHANNELS_Scanlists(i) & scanlistMask) ||
                         isEmptyChannelToSave;
    if (isOurScanlist) {
      gScanlist[gScanlistSize] = i;
      gScanlistSize++;
      // Log("Load CH %u in SL", i);
    }
  }
  if (typeFilter == TYPE_FILTER_CH || typeFilter == TYPE_FILTER_CH_SAVE) {
    chScanlistIndex = 0;
    CHANNELS_SetScanlistIndexFromRadio();
  }
  Log("SL sz: %u", gScanlistSize);
}

void CHANNELS_LoadBlacklistToLoot() {
  const uint16_t SCANLIST_MASK = 1 << 15;
  for (int16_t i = 0; i < CHANNELS_GetCountMax(); ++i) {
    if (CHANNELS_GetMeta(i).type == TYPE_CH &&
        (CHANNELS_Scanlists(i) & SCANLIST_MASK)) {
      CH ch;
      CHANNELS_Load(i, &ch);
      Loot *loot = LOOT_AddEx(ch.rxF, true);
      loot->open = false;
      loot->blacklist = true;
      loot->lastTimeOpen = 0;
    }
  }
}

uint16_t CHANNELS_GetStepSize(CH *p) { return StepFrequencyTable[p->step]; }

uint32_t CHANNELS_GetSteps(CH *p) {
  return (p->txF - p->rxF) / CHANNELS_GetStepSize(p) + 1;
}

uint32_t CHANNELS_GetF(Band *p, uint32_t channel) {
  return p->rxF + channel * CHANNELS_GetStepSize(p);
}

uint32_t CHANNELS_GetChannel(CH *p, uint32_t f) {
  return (f - p->rxF) / CHANNELS_GetStepSize(p);
}

CHMeta CHANNELS_GetMeta(int16_t num) {
  CHMeta meta;
  EEPROM_ReadBuffer(GetChannelOffset(num) + offsetof(CH, meta), &meta, 1);
  return meta;
}

bool CHANNELS_IsScanlistable(CHType type) {
  return type == TYPE_CH || type == TYPE_BAND || type == TYPE_FOLDER ||
         type == TYPE_EMPTY;
}

bool CHANNELS_IsFreqable(CHType type) {
  return type == TYPE_CH || type == TYPE_BAND;
}

uint16_t CHANNELS_ScanlistByKey(uint16_t sl, KEY_Code_t key, bool longPress) {
  if (key >= KEY_1 && key <= KEY_8) {
    return sl ^ 1 << ((key - 1) + (longPress ? 8 : 0));
  } else {
    return SCANLIST_ALL;
  }
}
