#ifndef SCAN_H
#define SCAN_H

#include "channels.h"
#include "lootlist.h"
#include <stdint.h>

typedef enum {
  SCAN_MODE_SINGLE,    // Одна частота (мониторинг)
  SCAN_MODE_CHANNEL,   // Канальный режим
  SCAN_MODE_FREQUENCY, // Частотный режим (диапазон)
  SCAN_MODE_ANALYSER // Режим анализатора (только частотный)
} ScanMode;

typedef enum {
  SCAN_STATE_IDLE,
  SCAN_STATE_SCANNING,
  SCAN_STATE_LISTENING,
  SCAN_STATE_PAUSED
} ScanStateType;

void SCAN_SetMode(ScanMode mode);
void SCAN_Init(bool multiband);
void SCAN_setStartF(uint32_t f);
void SCAN_setEndF(uint32_t f);
void SCAN_setRange(uint32_t fs, uint32_t fe);
void SCAN_setBand(Band b);
void SCAN_Check();
void SCAN_Next();
uint32_t SCAN_GetCps();
void SCAN_NextBlacklist();
void SCAN_NextWhitelist();

void SCAN_SetDelay(uint32_t delay);
uint32_t SCAN_GetDelay();

#endif /* end of include guard: SCAN_H */
