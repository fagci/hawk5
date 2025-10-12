#include "scan.h"
#include "../apps/apps.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../driver/uart.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/spectrum.h"
#include "bands.h"

// =============================
// Состояние сканирования
// =============================
typedef struct {
  uint32_t scanDelayUs; // Задержка измерения (микросек)
  uint32_t stayAtTimeout; // Таймаут удержания на частоте
  uint32_t scanListenTimeout; // Таймаут прослушивания
  uint32_t scanCycles; // Количество циклов сканирования
  uint32_t lastCpsTime;    // Последнее время замера CPS
  uint32_t lastRenderTime; // Последнее время отрисовки
  uint16_t squelchLevel; // Текущий уровень шумоподавления
  bool thinking;         // Думоем
  bool wasThinkingEarlier; // Флаг для корректировки squelch
  bool lastListenState;    // Последнее состояние squelch
  bool isMultiband;        // Мультидиапазонный режим
} ScanState;

static ScanState scan = {
    .scanDelayUs = 1500,
    .squelchLevel = 0,
    .thinking = false,
    .wasThinkingEarlier = false,
    .lastListenState = false,
    .isMultiband = false,
    .stayAtTimeout = 0,
    .scanListenTimeout = 0,
    .scanCycles = 0,
    .lastCpsTime = 0,
    .lastRenderTime = 0,
};

// =============================
// Вспомогательные функции
// =============================

static uint16_t MeasureSignal(uint32_t frequency, bool precise) {
  RADIO_SetParam(ctx, PARAM_PRECISE_F_CHANGE, precise, false);
  RADIO_SetParam(ctx, PARAM_FREQUENCY, frequency, false);
  RADIO_ApplySettings(ctx);
  SYSTICK_DelayUs(precise ? scan.scanDelayUs : 0);
  return RADIO_GetRSSI(ctx);
}

static void ApplyBandSettings() {
  vfo->msm.f = gCurrentBand.rxF;
  RADIO_SetParam(ctx, PARAM_STEP, gCurrentBand.step, false);
  RADIO_ApplySettings(ctx);
  SP_Init(&gCurrentBand);
  LogC(LOG_C_BRIGHT_YELLOW, "[SCANER] Bounds: %u .. %u", gCurrentBand.rxF,
       gCurrentBand.txF);
}

static void NextFrequency() {
  // TODO: priority cooldown scan
  uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];
  vfo->msm.f += step;
  if (vfo->is_open) {
    vfo->is_open = false;
    RADIO_SwitchAudioToVFO(&gRadioState, gRadioState.active_vfo_index);
  }

  if (vfo->msm.f > gCurrentBand.txF) {
    if (scan.isMultiband) {
      BANDS_SelectBandRelativeByScanlist(true);
      ApplyBandSettings();
    }
    vfo->msm.f = gCurrentBand.rxF;
    gRedrawScreen = true;
  }

  LOOT_Replace(&vfo->msm, vfo->msm.f);
  SetTimeout(&scan.scanListenTimeout, 0);
  SetTimeout(&scan.stayAtTimeout, 0);
  scan.scanCycles++;
}

static void NextWithTimeout() {
  if (scan.lastListenState != vfo->is_open) {
    scan.lastListenState = vfo->is_open;

    if (vfo->is_open) {
      SetTimeout(&scan.scanListenTimeout,
                 SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]);
      SetTimeout(&scan.stayAtTimeout, UINT32_MAX);
    } else {
      SetTimeout(&scan.stayAtTimeout, SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
    }
  }

  if ((CheckTimeout(&scan.scanListenTimeout) && vfo->is_open) ||
      CheckTimeout(&scan.stayAtTimeout)) {
    NextFrequency();
  }
}

// =============================
// API функций
// =============================
uint32_t SCAN_GetCps() {
  uint32_t cps = scan.scanCycles * 1000 / (Now() - scan.lastCpsTime);
  scan.lastCpsTime = Now();
  scan.scanCycles = 0;
  return cps;
}

void SCAN_setBand(Band b) {
  gCurrentBand = b;
  ApplyBandSettings();
}

void SCAN_setStartF(uint32_t f) {
  gCurrentBand.rxF = f;
  ApplyBandSettings();
}

void SCAN_setEndF(uint32_t f) {
  gCurrentBand.txF = f;
  ApplyBandSettings();
}

void SCAN_setRange(uint32_t fs, uint32_t fe) {
  gCurrentBand.rxF = fs;
  gCurrentBand.txF = fe;
  ApplyBandSettings();
}

void SCAN_Next(bool up) { NextFrequency(); }

void SCAN_Init(bool multiband) {
  scan.isMultiband = multiband;
  vfo->msm.snr = 0;
  scan.lastCpsTime = Now();
  scan.scanCycles = 0;
  ApplyBandSettings();
}

// =============================
// Обработка сканирования
// =============================
static void HandleAnalyserMode() {
  vfo->msm.rssi = MeasureSignal(vfo->msm.f, false);
  SP_AddPoint(&vfo->msm);
  if (Now() - scan.lastRenderTime > 500) {
    gRedrawScreen = true;
    scan.lastRenderTime = Now();
  }
  NextFrequency();
}

static void UpdateSquelchAndRssi(bool isAnalyserMode) {
  if (gSettings.skipGarbageFrequencies &&
      (RADIO_GetParam(&vfo->context, PARAM_FREQUENCY) % GARBAGE_FREQUENCY_MOD ==
       0)) {
    vfo->msm.open = false;
    vfo->msm.rssi = 0;
    SP_AddPoint(&vfo->msm);
    return;
  }
  vfo->msm.rssi = MeasureSignal(vfo->msm.f, !isAnalyserMode);

  if (!scan.squelchLevel && vfo->msm.rssi) {
    scan.squelchLevel = vfo->msm.rssi - 1;
  }

  if (scan.squelchLevel > vfo->msm.rssi) {
    uint16_t perc = (scan.squelchLevel - vfo->msm.rssi) * 100 /
                    ((scan.squelchLevel + vfo->msm.rssi) / 2);
    if (perc >= 25) {
      scan.squelchLevel = vfo->msm.rssi - 1;
    }
  }

  vfo->msm.open = vfo->msm.rssi >= scan.squelchLevel;
  SP_AddPoint(&vfo->msm);
}

void SCAN_Check(bool isAnalyserMode) {
  RADIO_UpdateMultiwatch(&gRadioState);
  RADIO_CheckAndSaveVFO(&gRadioState);

  if (isAnalyserMode) {
    HandleAnalyserMode();
    return;
  }

  if (vfo->msm.open) {
    RADIO_UpdateSquelch(&gRadioState);
    vfo->msm.open = vfo->is_open;
    gRedrawScreen = true;
  } else {
    UpdateSquelchAndRssi(isAnalyserMode);
  }

  if (vfo->msm.open && !vfo->is_open) {
    scan.thinking = true;
    scan.wasThinkingEarlier = true;
    SYS_DelayMs(SQL_DELAY);
    RADIO_UpdateSquelch(&gRadioState);
    vfo->msm.open = vfo->is_open;
    scan.thinking = false;
    gRedrawScreen = true;
    if (!vfo->msm.open) {
      scan.squelchLevel++;
    }
  }

  LOOT_Update(&vfo->msm);

  if (vfo->is_open && !vfo->msm.open) {
    scan.squelchLevel = SP_GetNoiseFloor();
  }

  if (vfo->msm.open) {
    gRedrawScreen = true;
  }

  static uint8_t stepsPassed;
  if (!vfo->msm.open) {
    if (stepsPassed++ > 64) {
      stepsPassed = 0;
      gRedrawScreen = true;
      if (!scan.wasThinkingEarlier) {
        scan.squelchLevel--;
      }
      scan.wasThinkingEarlier = false;
    }
  }

  NextWithTimeout();
}

void SCAN_SetDelay(uint32_t delay) { scan.scanDelayUs = delay; }
uint32_t SCAN_GetDelay() { return scan.scanDelayUs; }
