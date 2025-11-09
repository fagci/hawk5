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
#include "channels.h"
#include "lootlist.h"

// =============================
// Состояние сканирования
// =============================
typedef struct {
  ScanMode mode;
  uint32_t scanDelayUs; // Задержка измерения (микросек)
  uint32_t stayAtTimeout; // Таймаут удержания на частоте
  uint32_t scanListenTimeout; // Таймаут прослушивания
  uint32_t scanCycles; // Количество циклов сканирования
  uint32_t lastCpsTime; // Последнее время замера CPS
  uint32_t currentCps; // Текущее значение CPS (кешированное)
  uint32_t cpsUpdateInterval; // Интервал обновления CPS (мс)
  uint16_t squelchLevel; // Текущий уровень шумоподавления

  bool thinking;           // Думоем
  bool wasThinkingEarlier; // Флаг для корректировки squelch
  bool lastListenState;    // Последнее состояние squelch
  bool isMultiband;        // Мультидиапазонный режим
} ScanState;

static ScanState scan = {
    .mode = SCAN_MODE_FREQUENCY,
    .scanDelayUs = 1200,
    .squelchLevel = 0,
    .thinking = false,
    .wasThinkingEarlier = false,
    .lastListenState = false,
    .isMultiband = false,
    .stayAtTimeout = 0,
    .scanListenTimeout = 0,
    .scanCycles = 0,
    .lastCpsTime = 0,
    .currentCps = 0,
    .cpsUpdateInterval = 1000,
};

// =============================
// Вспомогательные функции
// =============================

static void UpdateCPS() {
  uint32_t now = Now();
  uint32_t elapsed = now - scan.lastCpsTime;

  // Обновляем CPS только если прошел достаточный интервал
  if (elapsed >= scan.cpsUpdateInterval) {
    if (elapsed > 0) {
      scan.currentCps = (scan.scanCycles * 1000) / elapsed;
    }
    scan.lastCpsTime = now;
    scan.scanCycles = 0;
  }
}

static uint16_t MeasureSignal(uint32_t frequency, bool precise) {
  RADIO_SetParam(ctx, PARAM_PRECISE_F_CHANGE, precise, false);
  RADIO_SetParam(ctx, PARAM_FREQUENCY, frequency, false);
  RADIO_ApplySettings(ctx);
  TIMER_DelayUs(precise ? scan.scanDelayUs : 50);
  return RADIO_GetRSSI(ctx);
}

static void ApplyBandSettings() {
  vfo->msm.f = gCurrentBand.rxF;

  RADIO_SetParam(ctx, PARAM_FREQUENCY, vfo->msm.f, false);
  RADIO_SetParam(ctx, PARAM_STEP, gCurrentBand.step, false);
  RADIO_ApplySettings(ctx);
  SP_Init(&gCurrentBand);
  LogC(LOG_C_BRIGHT_YELLOW, "[SCANER] Bounds: %u .. %u", gCurrentBand.rxF,
       gCurrentBand.txF);
  if (gLastActiveLoot && !BANDS_InRange(gLastActiveLoot->f, gCurrentBand)) {
    gLastActiveLoot = NULL;
  }
}

static void NextFrequency() {
  // TODO: priority cooldown scan
  uint32_t step = StepFrequencyTable[RADIO_GetParam(ctx, PARAM_STEP)];
  vfo->msm.f += step;
  if (vfo->is_open) {
    vfo->is_open = false;
    RADIO_SwitchAudioToVFO(gRadioState, gRadioState->active_vfo_index);
  }

  if (vfo->msm.f > gCurrentBand.txF) {
    if (scan.isMultiband) {
      BANDS_SelectBandRelativeByScanlist(true);
      ApplyBandSettings();
    }
    vfo->msm.f = gCurrentBand.rxF;
    gRedrawScreen = true;
  } else if (vfo->msm.f < gCurrentBand.rxF) {
    vfo->msm.f = gCurrentBand.txF;
    gRedrawScreen = true;
  }

  LOOT_Replace(&vfo->msm, vfo->msm.f);
  SetTimeout(&scan.scanListenTimeout, 0);
  SetTimeout(&scan.stayAtTimeout, 0);
  UpdateCPS();
}

static void NextStep() {
  switch (scan.mode) {
  case SCAN_MODE_SINGLE:
    // Остаемся на текущей частоте
    // Только обновляем squelch
    break;

  case SCAN_MODE_CHANNEL:
    // Переход к следующему каналу
    CHANNELS_Next(true);
    CHANNELS_LoadCurrentScanlistCH();
    LOOT_Replace(&vfo->msm, vfo->msm.f);
    break;

  case SCAN_MODE_FREQUENCY:
  case SCAN_MODE_ANALYSER:
    // Переход к следующей частоте (шаг)
    NextFrequency();
    break;
  }

  SetTimeout(&scan.scanListenTimeout, 0);
  SetTimeout(&scan.stayAtTimeout, 0);
  scan.scanCycles++;
  UpdateCPS();
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

// API для установки режима
void SCAN_SetMode(ScanMode mode) {
  scan.mode = mode;

  // Сброс состояния при смене режима
  scan.scanCycles = 0;
  scan.squelchLevel = 0;
  scan.thinking = false;
  SetTimeout(&scan.stayAtTimeout, 0);
  SetTimeout(&scan.scanListenTimeout, 0);

  // Специфичная инициализация для режима
  switch (mode) {
  case SCAN_MODE_SINGLE:
    // Не двигаемся с частоты, только мониторим
    break;
  case SCAN_MODE_CHANNEL:
    // Загрузим первый канал из списка
    CHANNELS_LoadCurrentScanlistCH();
    break;
  case SCAN_MODE_FREQUENCY:
  case SCAN_MODE_ANALYSER:
    // Установим границы диапазона
    ApplyBandSettings();
    break;
  }
}

ScanMode SCAN_GetMode(void) { return scan.mode; }

uint32_t SCAN_GetCps() { return scan.currentCps; }

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

void SCAN_Next() { NextFrequency(); }

void SCAN_NextBlacklist() {
  LOOT_BlacklistLast();
  SCAN_Next();
}

void SCAN_NextWhitelist() {
  LOOT_BlacklistLast();
  SCAN_Next();
}

void SCAN_Init(bool multiband) {
  scan.isMultiband = multiband;
  vfo->msm.snr = 0;
  scan.lastCpsTime = Now();
  scan.scanCycles = 0;
  scan.currentCps = 0;

  // CHANNELS_LoadBlacklistToLoot();

  ApplyBandSettings();
  BK4819_WriteRegister(BK4819_REG_3F, 0);
}

// =============================
// Обработка сканирования
// =============================
static void HandleAnalyserMode() {
  vfo->msm.rssi = MeasureSignal(vfo->msm.f, false);
  scan.scanCycles++;
  SP_AddPoint(&vfo->msm);
  NextFrequency();
}

static void UpdateSquelchAndRssi(bool isAnalyserMode) {
  Loot *msm = LOOT_Get(vfo->msm.f);
  if ((gSettings.skipGarbageFrequencies &&
       (vfo->msm.f % GARBAGE_FREQUENCY_MOD == 0)) ||
      (msm && (msm->blacklist || msm->whitelist))) {
    vfo->msm.open = false;
    vfo->msm.rssi = 0;
    SP_AddPoint(&vfo->msm);
    NextFrequency();
    return;
  }
  vfo->msm.rssi = MeasureSignal(vfo->msm.f, !isAnalyserMode);
  scan.scanCycles++;

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

void SCAN_Check() {
  RADIO_UpdateMultiwatch(gRadioState);

  // Режим анализатора — упрощенная логика
  if (scan.mode == SCAN_MODE_ANALYSER) {
    vfo->msm.rssi = MeasureSignal(vfo->msm.f, false);
    SP_AddPoint(&vfo->msm);
    NextStep();
    return;
  }

  // Одиночная частота — только мониторинг
  if (scan.mode == SCAN_MODE_SINGLE) {
    /* RADIO_UpdateSquelch(gRadioState);
    vfo->msm.rssi = MeasureSignal(vfo->msm.f, true);
    vfo->msm.open = vfo->is_open;
    gRedrawScreen = true; */

    static uint32_t radioTimer;
    RADIO_CheckAndSaveVFO(gRadioState);
    if (Now() - radioTimer >= SQL_DELAY) {
      RADIO_UpdateSquelch(gRadioState);
      SP_ShiftGraph(-1);
      SP_AddGraphPoint(&vfo->msm);
      radioTimer = Now();
    }

    return;
  }

  // Общая логика для канального и частотного режимов
  if (vfo->msm.open) {
    RADIO_UpdateSquelch(gRadioState);
    vfo->msm.open = vfo->is_open;
    gRedrawScreen = true;
  } else {
    UpdateSquelchAndRssi(scan.mode == SCAN_MODE_ANALYSER);
  }

  // Проверка на "думание" о squelch
  if (vfo->msm.open && !vfo->is_open) {
    scan.thinking = true;
    scan.wasThinkingEarlier = true;
    TIMER_DelayUs(SQL_DELAY * 1000);
    RADIO_UpdateSquelch(gRadioState);
    vfo->msm.open = vfo->is_open;
    scan.thinking = false;

    if (!vfo->msm.open) {
      scan.squelchLevel++;
    }
  }

  LOOT_Update(&vfo->msm);

  // Автокоррекция squelch
  if (vfo->is_open && !vfo->msm.open) {
    scan.squelchLevel = SP_GetNoiseFloor();
  }

  // Переход к следующей частоте/каналу с учетом таймаутов
  NextWithTimeout();
}

/* void SCAN_Check(bool isAnalyserMode) {
  static uint32_t lastFreqChangeTime = 0;
  static uint32_t stuckCounter = 0;

  // Проверка на зависание на одной частоте
  if (Now() - lastFreqChangeTime > 5000) { // 5 секунд без смены частоты
    stuckCounter++;
    if (stuckCounter > 3) {
      scan.squelchLevel = 0; // Сброс squelch
      NextFrequency();
      stuckCounter = 0;
    }
    lastFreqChangeTime = Now();
  }

  RADIO_UpdateMultiwatch(gRadioState);

  if (isAnalyserMode) {
    HandleAnalyserMode();
    return;
  }

  if (vfo->msm.open) {
    RADIO_UpdateSquelch(gRadioState);
    vfo->msm.open = vfo->is_open;
    gRedrawScreen = true;
  } else {
    UpdateSquelchAndRssi(isAnalyserMode);
  }

  if (vfo->msm.open && !vfo->is_open) {
    scan.thinking = true;
    scan.wasThinkingEarlier = true;
    TIMER_DelayUs(SQL_DELAY * 1000);
    RADIO_UpdateSquelch(gRadioState);
    vfo->msm.open = vfo->is_open;
    scan.thinking = false;
    // gRedrawScreen = true;
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
      // gRedrawScreen = true;
      if (!scan.wasThinkingEarlier && scan.squelchLevel > 0) {
        scan.squelchLevel--;
      }
      scan.wasThinkingEarlier = false;
    }
  } else {
    stepsPassed = 0;
  }

  NextWithTimeout();
} */

void SCAN_SetDelay(uint32_t delay) { scan.scanDelayUs = delay; }
uint32_t SCAN_GetDelay() { return scan.scanDelayUs; }
