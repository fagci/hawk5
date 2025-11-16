#pragma once
#include "RadioCommon.hpp"

// ============================================================================
// МОДУЛЯЦИЯ - названия из radio.c
// ============================================================================

static const char *MODULATION_NAMES[] = {
    "FM",  // 0 - MOD_FM
    "AM",  // 1 - MOD_AM
    "LSB", // 2 - MOD_LSB
    "USB", // 3 - MOD_USB
    "BYP", // 4 - MOD_BYP
    "RAW", // 5 - MOD_RAW
    "WFM"  // 6 - MOD_WFM
};

// ============================================================================
// BK4819 - полоса пропускания из bk4819.h
// ============================================================================

// Из enum BK4819_FilterBandwidth_t:
// BK4819_FILTER_BW_6k,   // U 6K
// BK4819_FILTER_BW_7k,   // U 7K
// BK4819_FILTER_BW_9k,   // N 9k
// BK4819_FILTER_BW_10k,  // N 10k
// BK4819_FILTER_BW_12k,  // W 12k
// BK4819_FILTER_BW_14k,  // W 14k
// BK4819_FILTER_BW_17k,  // W 17k
// BK4819_FILTER_BW_20k,  // W 20k
// BK4819_FILTER_BW_23k,  // W 23k
// BK4819_FILTER_BW_26k,  // W 26k

static const uint32_t BK4819_BANDWIDTH_HZ[] = {
    6000,  // 0 - 6K (U - Узкая)
    7000,  // 1 - 7K (U)
    9000,  // 2 - 9k (N - Нормальная)
    10000, // 3 - 10k (N)
    12000, // 4 - 12k (W - Широкая)
    14000, // 5 - 14k (W)
    17000, // 6 - 17k (W)
    20000, // 7 - 20k (W)
    23000, // 8 - 23k (W)
    26000  // 9 - 26k (W)
};

static const char *BK4819_BANDWIDTH_NAMES[] = {
    "U6K",  // 0
    "U7K",  // 1
    "N9k",  // 2
    "N10k", // 3
    "W12k", // 4
    "W14k", // 5
    "W17k", // 6
    "W20k", // 7
    "W23k", // 8
    "W26k"  // 9
};

// ============================================================================
// SI4732 - полоса пропускания AM из radio.c
// ============================================================================

// const char *BW_NAMES_SI47XX[7] = {"6k", "4k", "3k", "2k", "1k", "1.8k",
// "2.5k"};

static const uint32_t SI4732_AM_BANDWIDTH_HZ[] = {
    6000, // 0 - 6k
    4000, // 1 - 4k
    3000, // 2 - 3k
    2000, // 3 - 2k
    1000, // 4 - 1k
    1800, // 5 - 1.8k
    2500  // 6 - 2.5k
};

static const char *SI4732_AM_BANDWIDTH_NAMES[] = {
    "6k",   // 0
    "4k",   // 1
    "3k",   // 2
    "2k",   // 3
    "1k",   // 4
    "1.8k", // 5
    "2.5k"  // 6
};

// ============================================================================
// SI4732 - полоса пропускания SSB из radio.c
// ============================================================================

// const char *BW_NAMES_SI47XX_SSB[6] = {"1.2k", "2.2k", "3k", "4k", "0.5k",
// "1.0k"};

static const uint32_t SI4732_SSB_BANDWIDTH_HZ[] = {
    1200, // 0 - 1.2k
    2200, // 1 - 2.2k
    3000, // 2 - 3k
    4000, // 3 - 4k
    500,  // 4 - 0.5k
    1000  // 5 - 1.0k
};

static const char *SI4732_SSB_BANDWIDTH_NAMES[] = {
    "1.2k", // 0
    "2.2k", // 1
    "3k",   // 2
    "4k",   // 3
    "0.5k", // 4
    "1.0k"  // 5
};

// ============================================================================
// ШАГ ЧАСТОТЫ - из radio.c
// ============================================================================

// const uint16_t StepFrequencyTable[15] = {
//   2, 5, 50, 100, 250, 500, 625, 833, 900, 1000, 1250, 2500, 5000, 10000,
//   50000,
// };

// Значения в единицах 10Hz!
static const uint32_t STEP_VALUES_10HZ[] = {
    2,     // 0 - 20 Hz
    5,     // 1 - 50 Hz
    50,    // 2 - 500 Hz
    100,   // 3 - 1 kHz
    250,   // 4 - 2.5 kHz
    500,   // 5 - 5 kHz
    625,   // 6 - 6.25 kHz
    833,   // 7 - 8.33 kHz
    900,   // 8 - 9 kHz
    1000,  // 9 - 10 kHz
    1250,  // 10 - 12.5 kHz
    2500,  // 11 - 25 kHz
    5000,  // 12 - 50 kHz
    10000, // 13 - 100 kHz
    50000  // 14 - 500 kHz
};

static const char *STEP_NAMES[] = {
    "20Hz",    // 0
    "50Hz",    // 1
    "500Hz",   // 2
    "1kHz",    // 3
    "2.5kHz",  // 4
    "5kHz",    // 5
    "6.25kHz", // 6
    "8.33kHz", // 7
    "9kHz",    // 8
    "10kHz",   // 9
    "12.5kHz", // 10
    "25kHz",   // 11
    "50kHz",   // 12
    "100kHz",  // 13
    "500kHz"   // 14
};

// ============================================================================
// TX POWER - названия из radio.c
// ============================================================================

// const char *TX_STATE_NAMES[7] = {
//   "TX Off", "TX On", "CHARGING", "BAT LOW", "DISABLED", "UPCONV", "HIGH POW",
// };

static const char *TX_POWER_NAMES[] = {
    "Low",    // 0
    "Mid",    // 1
    "High",   // 2
    "Higher", // 3
    "Highest" // 4
};

static const char *TX_STATE_NAMES[] = {
    "TX Off", "TX On", "CHARGING", "BAT LOW", "DISABLED", "UPCONV", "HIGH POW"};

// ============================================================================
// SQUELCH TYPE - из radio.c
// ============================================================================

// const char *SQ_TYPE_NAMES[4] = {"RNG", "RG", "RN", "R"};

static const char *SQUELCH_TYPE_NAMES[] = {
    "RNG", // 0 - SQUELCH_RSSI_NOISE_GLITCH
    "RG",  // 1 - SQUELCH_RSSI_GLITCH
    "RN",  // 2 - SQUELCH_RSSI_NOISE
    "R"    // 3 - SQUELCH_RSSI
};

// ============================================================================
// FILTER - названия из radio.c
// ============================================================================

// const char *FILTER_NAMES[4] = {
//   "VHF",  // FILTER_VHF = 0
//   "UHF",  // FILTER_UHF = 1
//   "Off",  // FILTER_OFF = 2
//   "Auto", // FILTER_AUTO = 3
// };

static const char *FILTER_NAMES[] = {
    "VHF", // 0
    "UHF", // 1
    "Off", // 2
    "Auto" // 3
};

// ============================================================================
// RADIO TYPE - названия из radio.c
// ============================================================================

// const char *RADIO_NAMES[3] = {"BK4819", "BK1080", "SI4732"};

static const char *RADIO_TYPE_NAMES[] = {
    "BK4819", // 0
    "BK1080", // 1
    "SI4732"  // 2
};

// ============================================================================
// XTAL MODE - кристалл
// ============================================================================

static const char *XTAL_MODE_NAMES[] = {
    "0.13M",  // 0 - XTAL_0_13M
    "1.192M", // 1 - XTAL_1_192M
    "2.26M",  // 2 - XTAL_2_26M (default)
    "3.384M"  // 3 - XTAL_3_384M
};

// ============================================================================
// HELPER ФУНКЦИИ
// ============================================================================

namespace ParamFormat {

// Получить название модуляции
inline const char *getModulationName(uint32_t mod) {
  if (mod < sizeof(MODULATION_NAMES) / sizeof(MODULATION_NAMES[0])) {
    return MODULATION_NAMES[mod];
  }
  return "???";
}

// Получить реальное значение полосы в Гц
inline uint32_t getBandwidthHz(RadioType radioType, uint32_t modulation,
                               uint32_t bwIndex) {
  switch (radioType) {
  case RadioType::BK4819:
    if (bwIndex <
        sizeof(BK4819_BANDWIDTH_HZ) / sizeof(BK4819_BANDWIDTH_HZ[0])) {
      return BK4819_BANDWIDTH_HZ[bwIndex];
    }
    break;

  case RadioType::SI4732:
    // SSB режимы (LSB/USB)
    if (modulation == (uint32_t)ModType::USB ||
        modulation == (uint32_t)ModType::LSB) {
      if (bwIndex < sizeof(SI4732_SSB_BANDWIDTH_HZ) /
                        sizeof(SI4732_SSB_BANDWIDTH_HZ[0])) {
        return SI4732_SSB_BANDWIDTH_HZ[bwIndex];
      }
    } else {
      // AM/FM режимы
      if (bwIndex <
          sizeof(SI4732_AM_BANDWIDTH_HZ) / sizeof(SI4732_AM_BANDWIDTH_HZ[0])) {
        return SI4732_AM_BANDWIDTH_HZ[bwIndex];
      }
    }
    break;

  default:
    break;
  }
  return 0;
}

// Получить название полосы
inline const char *getBandwidthName(RadioType radioType, uint32_t modulation,
                                    uint32_t bwIndex) {
  switch (radioType) {
  case RadioType::BK4819:
    if (bwIndex <
        sizeof(BK4819_BANDWIDTH_NAMES) / sizeof(BK4819_BANDWIDTH_NAMES[0])) {
      return BK4819_BANDWIDTH_NAMES[bwIndex];
    }
    break;

  case RadioType::SI4732:
    if (modulation == (uint32_t)ModType::USB ||
        modulation == (uint32_t)ModType::LSB) {
      if (bwIndex < sizeof(SI4732_SSB_BANDWIDTH_NAMES) /
                        sizeof(SI4732_SSB_BANDWIDTH_NAMES[0])) {
        return SI4732_SSB_BANDWIDTH_NAMES[bwIndex];
      }
    } else {
      if (bwIndex < sizeof(SI4732_AM_BANDWIDTH_NAMES) /
                        sizeof(SI4732_AM_BANDWIDTH_NAMES[0])) {
        return SI4732_AM_BANDWIDTH_NAMES[bwIndex];
      }
    }
    break;

  default:
    break;
  }
  return "???";
}

// Получить реальное значение шага в 10Hz
inline uint32_t getStepValue(uint32_t stepIndex) {
  if (stepIndex < sizeof(STEP_VALUES_10HZ) / sizeof(STEP_VALUES_10HZ[0])) {
    return STEP_VALUES_10HZ[stepIndex];
  }
  return 2500; // По умолчанию 25 kHz (индекс 11)
}

// Получить название шага
inline const char *getStepName(uint32_t stepIndex) {
  if (stepIndex < sizeof(STEP_NAMES) / sizeof(STEP_NAMES[0])) {
    return STEP_NAMES[stepIndex];
  }
  return "???";
}

// Получить название типа squelch
inline const char *getSquelchTypeName(uint32_t sqType) {
  if (sqType < sizeof(SQUELCH_TYPE_NAMES) / sizeof(SQUELCH_TYPE_NAMES[0])) {
    return SQUELCH_TYPE_NAMES[sqType];
  }
  return "???";
}

// Получить название фильтра
inline const char *getFilterName(uint32_t filter) {
  if (filter < sizeof(FILTER_NAMES) / sizeof(FILTER_NAMES[0])) {
    return FILTER_NAMES[filter];
  }
  return "???";
}

// Получить название радио
inline const char *getRadioTypeName(RadioType radioType) {
  uint32_t idx = (uint32_t)radioType;
  if (idx < sizeof(RADIO_TYPE_NAMES) / sizeof(RADIO_TYPE_NAMES[0])) {
    return RADIO_TYPE_NAMES[idx];
  }
  return "???";
}

// Получить название TX power
inline const char *getTxPowerName(uint32_t power) {
  if (power < sizeof(TX_POWER_NAMES) / sizeof(TX_POWER_NAMES[0])) {
    return TX_POWER_NAMES[power];
  }
  return "???";
}

inline const char *getTxStateName(uint32_t txState) {
  if (txState < sizeof(TX_STATE_NAMES) / sizeof(TX_STATE_NAMES[0]))
    return TX_STATE_NAMES[txState];
  return "???";
}

// Получить название XTAL mode
inline const char *getXtalModeName(uint32_t xtal) {
  if (xtal < sizeof(XTAL_MODE_NAMES) / sizeof(XTAL_MODE_NAMES[0])) {
    return XTAL_MODE_NAMES[xtal];
  }
  return "???";
}

// Форматировать частоту в строку (145.500 MHz)
inline void formatFrequency(uint32_t freq10Hz, char *buf, size_t bufSize) {
  uint32_t freqHz = freq10Hz * 10;
  uint32_t mhz = freqHz / 1000000;
  uint32_t khz = (freqHz % 1000000) / 1000;
  uint32_t hz = freqHz % 1000;

  if (hz != 0) {
    snprintf(buf, bufSize, "%u.%03u.%03u", mhz, khz, hz);
  } else {
    snprintf(buf, bufSize, "%u.%03u", mhz, khz);
  }
}

// Форматировать RSSI в dBm (из radio.c: Rssi2DBm)
inline int32_t rssiToDbm(uint32_t rssi) {
  // Из функции Rssi2DBm в radio.c
  // Простая линейная аппроксимация
  return -130 + ((int32_t)rssi / 2);
}

} // namespace ParamFormat
