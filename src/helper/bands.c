#include "bands.h"
#include "../driver/uart.h"
#include "../external/printf/printf.h"
#include "../radio.h"
#include "channels.h"
#include "measurements.h"
#include <stdint.h>

// NOTE
// for SCAN use cached band by index
// for DISPLAY use bands in memory to select it by frequency faster

Band gCurrentBand;

// to use instead of predefined when we need to keep step, etc
Band defaultBand = {
    .meta.readonly = true,
    .meta.type = TYPE_BAND_DETACHED,
    .name = "Unknown",

    // .radio = RADIO_BK4819,
    .step = STEP_25_0kHz,
    .bw = BK4819_FILTER_BW_12k,

    .rxF = BK4819_F_MIN,
    .txF = BK4819_F_MAX,

    .squelch =
        {
            .type = SQUELCH_RSSI_NOISE_GLITCH,
            .value = 4,
        },
    .gainIndex = AUTO_GAIN_INDEX,
    // .allowTx = false,
};

static DBand allBands[BANDS_COUNT_MAX];
static int16_t allBandIndex; // -1 if default is current
static uint8_t allBandsSize = 0;

static uint8_t scanlistBandIndex;

static Band rangesStack[RANGES_STACK_SIZE] = {0};
static int8_t rangesStackIndex = -1;

/**
 * @see
 * https://github.com/fagci/s0v4/blob/f22fdbab201ec5ecc291f4d46e36e08e2bd370bd/src/helper/bands.c#L49
 * for different configurations
 */

static const PowerCalibration DEFAULT_POWER_CALIB = {43, 68, 140};

PCal POWER_CALIBRATIONS[] = {
    {.s = 135 * MHZ, .e = 165 * MHZ, .c = {38, 65, 140}},
    {.s = 165 * MHZ, .e = 205 * MHZ, .c = {36, 52, 140}},
    {.s = 205 * MHZ, .e = 215 * MHZ, .c = {41, 64, 135}},
    {.s = 215 * MHZ, .e = 220 * MHZ, .c = {44, 46, 50}},
    {.s = 220 * MHZ, .e = 240 * MHZ, .c = {0, 0, 0}},
    {.s = 240 * MHZ, .e = 265 * MHZ, .c = {62, 82, 130}},
    {.s = 265 * MHZ, .e = 270 * MHZ, .c = {65, 92, 140}},
    {.s = 270 * MHZ, .e = 275 * MHZ, .c = {73, 103, 140}},
    {.s = 275 * MHZ, .e = 285 * MHZ, .c = {81, 107, 140}},
    {.s = 285 * MHZ, .e = 295 * MHZ, .c = {57, 94, 140}},
    {.s = 295 * MHZ, .e = 305 * MHZ, .c = {74, 104, 140}},
    {.s = 305 * MHZ, .e = 335 * MHZ, .c = {81, 107, 140}},
    {.s = 335 * MHZ, .e = 345 * MHZ, .c = {63, 98, 140}},
    {.s = 345 * MHZ, .e = 355 * MHZ, .c = {52, 89, 140}},
    {.s = 355 * MHZ, .e = 365 * MHZ, .c = {46, 74, 140}},
    {.s = 470 * MHZ, .e = 620 * MHZ, .c = {46, 77, 140}},
};

static int16_t bandIndexByFreq(uint32_t f, bool preciseStep) {
  int16_t newBandIndex = -1;
  uint32_t smallestDiff = UINT32_MAX;
  for (uint8_t i = 0; i < allBandsSize; ++i) {
    DBand *b = &allBands[i];
    if (f < b->s || f > b->e) {
      continue;
    }
    if (preciseStep & (f % StepFrequencyTable[b->step])) {
      continue;
    }
    uint32_t diff = DeltaF(b->s, f) + DeltaF(b->e, f);
    if (diff < smallestDiff) {
      smallestDiff = diff;
      newBandIndex = i;
    }
  }
  return newBandIndex;
}

void BANDS_Load(void) {
  for (int16_t chNum = 0; chNum < CHANNELS_GetCountMax() - 2; ++chNum) {
    if (CHANNELS_GetMeta(chNum).type != TYPE_BAND) {
      continue;
    }

    CH ch;
    CHANNELS_Load(chNum, &ch);
    allBands[allBandsSize] = (DBand){
        .mr = chNum,
        .s = ch.rxF,
        .e = ch.txF,
        .step = ch.step,
    };

    allBandsSize++;

    if (allBandsSize >= BANDS_COUNT_MAX) {
      break;
    }
  }
}

bool BANDS_InRange(const uint32_t f, const Band p) {
  return f >= p.rxF && f <= p.txF;
}

void BANDS_SetRadioParamsFromCurrentBand() {
  RADIO_SetParam(ctx, PARAM_STEP, gCurrentBand.step, true);
  RADIO_SetParam(ctx, PARAM_BANDWIDTH, gCurrentBand.bw, true);
  RADIO_SetParam(ctx, PARAM_GAIN, gCurrentBand.gainIndex, true);
  RADIO_SetParam(ctx, PARAM_MODULATION, gCurrentBand.modulation, true);
  RADIO_SetParam(ctx, PARAM_SQUELCH_TYPE, gCurrentBand.squelch.type, true);
  RADIO_SetParam(ctx, PARAM_SQUELCH_VALUE, gCurrentBand.squelch.value, true);
  RADIO_SetParam(ctx, PARAM_RADIO, gCurrentBand.radio, true);
  RADIO_ApplySettings(ctx);
}

// Set gCurrentBand, sets internal cursor in SL
void BANDS_Select(int16_t num, bool copyToVfo) {
  CHANNELS_Load(num, &gCurrentBand);
  Log("Select Band %s", gCurrentBand.name);
  for (int16_t i = 0; i < gScanlistSize; ++i) {
    if (gScanlist[i] == num) {
      scanlistBandIndex = i;
      allBandIndex = bandIndexByFreq(gCurrentBand.rxF, true);
      // Log("SL band index %u", i);
      break;
    }
  }
  if (!BANDS_InRange(ctx->frequency, gCurrentBand)) {
    // Log("[BAND] !in range");
    uint32_t f = gCurrentBand.misc.lastUsedFreq;
    if (!f) {
      f = gCurrentBand.rxF;
      gCurrentBand.misc.lastUsedFreq = f;
    }
    RADIO_SetParam(ctx, PARAM_FREQUENCY, f, true);
  }
  // radio.allowTx = gCurrentBand.allowTx;
  if (copyToVfo) {
    BANDS_SetRadioParamsFromCurrentBand();
  }
}

// Used in vfo1 to select first band from scanlist
void BANDS_SelectScan(int8_t i) {
  if (gScanlistSize) {
    scanlistBandIndex = i;
    // RADIO_TuneToBand(gScanlist[i]);
  }
}

Band BANDS_ByFrequency(uint32_t f) {
  int16_t index = bandIndexByFreq(f, false);
  if (index >= 0) {
    Band b;
    CHANNELS_Load(allBands[index].mr, &b);
    return b;
  }
  return defaultBand;
}

uint8_t BANDS_GetScanlistIndex() { return scanlistBandIndex; }

/**
 * Select band, return if changed
 */
bool BANDS_SelectByFrequency(uint32_t f, bool copyToVfo) {
  int16_t newBandIndex = bandIndexByFreq(f, false);
  if (allBandIndex != newBandIndex ||
      gCurrentBand.meta.type == TYPE_BAND_DETACHED) {
    allBandIndex = newBandIndex;
    if (allBandIndex >= 0) {
      BANDS_Select(allBands[allBandIndex].mr, copyToVfo);
    } else {
      gCurrentBand = defaultBand;
    }
    return true;
  }
  return false;
}

bool BANDS_SelectBandRelativeByScanlist(bool next) {
  if (gScanlistSize == 0) {
    return false;
  }
  uint8_t oldScanlistBandIndex = scanlistBandIndex;
  scanlistBandIndex = IncDecU(scanlistBandIndex, 0, gScanlistSize, next);
  BANDS_Select(gScanlist[scanlistBandIndex], true);
  return oldScanlistBandIndex != scanlistBandIndex;
}

void BANDS_SaveCurrent(void) {
  // Log("BAND save i=%u, mr=%u", allBandIndex, allBands[allBandIndex].mr);
  if (allBandIndex >= 0 && gCurrentBand.meta.type == TYPE_BAND) {
    CHANNELS_Save(allBands[allBandIndex].mr, &gCurrentBand);
  }
}

PowerCalibration BANDS_GetPowerCalib(uint32_t f) {
  Band b = BANDS_ByFrequency(f);

  // not TYPE_BAND_DETACHED
  if (b.meta.type == TYPE_BAND && b.misc.powCalib.e > 0) {
    return b.misc.powCalib;
  }

  for (uint8_t ci = 0; ci < ARRAY_SIZE(POWER_CALIBRATIONS); ++ci) {
    PCal cal = POWER_CALIBRATIONS[ci];
    if (cal.s <= f && f < cal.e) {
      return cal.c;
    }
  }

  return DEFAULT_POWER_CALIB;
}

void BANDS_RangeClear() { rangesStackIndex = -1; }
int8_t BANDS_RangeIndex() { return rangesStackIndex; }

bool BANDS_RangePush(Band r) {
  if (rangesStackIndex < RANGES_STACK_SIZE - 1) {
    // Log("range +");
    rangesStack[++rangesStackIndex] = r;
  }
  return true;
}

Band BANDS_RangePop(void) {
  if (rangesStackIndex > 0) {
    // Log("range -");
    return rangesStack[rangesStackIndex--];
  }
  return rangesStack[rangesStackIndex];
}

Band *BANDS_RangePeek(void) {
  if (rangesStackIndex >= 0) {
    // Log("range peek ok");
    return &rangesStack[rangesStackIndex];
  }
  Log("NUL");
  return NULL;
}

uint8_t BANDS_CalculateOutputPower(TXOutputPower power, uint32_t f) {
  uint8_t power_bias;
  PowerCalibration cal = BANDS_GetPowerCalib(f);

  switch (power) {
  case TX_POW_LOW:
    power_bias = cal.s;
    break;

  case TX_POW_MID:
    power_bias = cal.m;
    break;

  case TX_POW_HIGH:
    power_bias = cal.e;
    break;

  default:
    power_bias = cal.s;
    if (power_bias > 10)
      power_bias -= 10; // 10mw if Low=500mw
  }

  return power_bias;
}
