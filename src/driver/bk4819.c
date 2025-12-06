#include "bk4819.h"
#include "../driver/gpio.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../driver/uart.h"
#include "../external/printf/printf.h"
#include "../inc/dp32g030/gpio.h"
#include "../inc/dp32g030/portcon.h"
#include "../misc.h"
#include "../settings.h"
#include "audio.h"
#include "bk4819-regs.h"
#include <stdint.h>
#include <stdio.h>

#define SHORT_DELAY()                                                          \
  __asm volatile("nop\n nop\n nop\n nop\n nop\n"                               \
                 "nop\n nop\n nop\n nop\n nop\n"                               \
                 "nop\n nop\n nop\n"); // ~13 NOPs для ~0.27 мкс @48MHz

// ============================================================================
// Constants
// ============================================================================

static const uint16_t MOD_TYPE_REG47_VALUES[] = {
    [MOD_FM] = BK4819_AF_FM,      [MOD_AM] = BK4819_AF_FM,
    [MOD_LSB] = BK4819_AF_USB,    [MOD_USB] = BK4819_AF_USB,
    [MOD_BYP] = BK4819_AF_BYPASS, [MOD_RAW] = BK4819_AF_RAW,
    [MOD_WFM] = BK4819_AF_FM,
};

static const uint8_t SQUELCH_TYPE_VALUES[4] = {0x88, 0xAA, 0xCC, 0xFF};

static const uint8_t DTMF_COEFFS[] = {111, 107, 103, 98, 80,  71,  58,  44,
                                      65,  55,  37,  23, 228, 203, 181, 159};

const Gain GAIN_TABLE[32] = {
    {0x1, 88},   //
    {0x9, 87},   //
    {0x2, 83},   //
    {0xA, 81},   //
    {0x12, 79},  //
    {0x2A, 77},  //
    {0x32, 75},  //
    {0x3A, 70},  //
    {0x20B, 68}, //
    {0x213, 64}, //
    {0x21B, 62}, //
    {0x214, 59}, //
    {0x21C, 56}, //
    {0x22D, 52}, //
    {0x23C, 50}, //
    {0x23D, 48}, //
    {0x255, 44}, //
    {0x25D, 42}, //
    {0x275, 39}, //
    {0x295, 33}, // Auto
    {0x295, 33}, //
    {0x2B6, 31}, //
    {0x354, 28}, // +5
    {0x36C, 23}, //
    {0x38C, 20}, //
    {0x38D, 17}, //
    {0x3B5, 13}, //
    {0x3B6, 9},  //
    {0x3D6, 8},  //
    {0x3BF, 3},  //
    {0x3DF, 2},  //
    {0x3FF, 0},  //

};

// AGC configuration constants
typedef struct {
  uint8_t lo;
  uint8_t low;
  uint8_t high;
} AgcConfig;

static const AgcConfig AGC_DEFAULT = {0, 56, 84};
static const AgcConfig AGC_FAST = {0, 20, 50};

// ============================================================================
// State Variables
// ============================================================================

static uint16_t gGpioOutState = 0x9000;
static uint8_t gSelectedFilter = 255;
static uint32_t gLastFrequency = 0;
static ModulationType gLastModulation = 255;

// ============================================================================
// Low-Level GPIO and SPI Operations
// ============================================================================

static inline void gpio_set_scn(bool high) {
  if (high) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  }
}

static inline void gpio_set_scl(bool high) {
  if (high) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  }
}

static inline void gpio_set_sda(bool high) {
  if (high) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
  }
}

static void spi_write_byte(uint8_t data) {
  gpio_set_scl(false);

  for (uint8_t i = 0; i < 8; i++) {
    gpio_set_sda(data & 0x80);
    SHORT_DELAY();
    gpio_set_scl(true);
    SHORT_DELAY();
    data <<= 1;
    gpio_set_scl(false);
    SHORT_DELAY();
  }
}

static uint16_t spi_read_word(void) {
  // Configure SDA as input
  PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) |
                     PORTCON_PORTC_IE_C2_BITS_ENABLE;
  GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;
  TIMER_DelayUs(1);

  uint16_t value = 0;
  for (uint8_t i = 0; i < 16; i++) {
    value <<= 1;
    value |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    gpio_set_scl(true);
    SHORT_DELAY();
    gpio_set_scl(false);
    SHORT_DELAY();
  }

  // Configure SDA back to output
  PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) |
                     PORTCON_PORTC_IE_C2_BITS_DISABLE;
  GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

  return value;
}

static void spi_write_word(uint16_t data) {
  gpio_set_scl(false);

  for (uint8_t i = 0; i < 16; i++) {
    gpio_set_sda(data & 0x8000);
    SHORT_DELAY();
    gpio_set_scl(true);
    data <<= 1;
    SHORT_DELAY();
    gpio_set_scl(false);
    SHORT_DELAY();
  }
}

// ============================================================================
// Register Access
// ============================================================================

static uint16_t reg30state = 0xffff;

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t reg) {
  if (reg == BK4819_REG_30 && reg30state != 0xffff) {
    return reg30state;
  }
  // Log("[BK] R 0x%02x", reg);
  gpio_set_scn(true);
  gpio_set_scl(false);
  // TIMER_DelayTicks(1);
  __asm volatile("nop \n"
                 "nop \n");
  gpio_set_scn(false);

  spi_write_byte(reg | 0x80);
  uint16_t value = spi_read_word();

  gpio_set_scn(true);
  // TIMER_DelayTicks(1);
  gpio_set_scl(true);
  gpio_set_sda(true);

  return value;
}

void BK4819_WriteRegister(BK4819_REGISTER_t reg, uint16_t data) {
  if (reg == BK4819_REG_30) {
    reg30state = data;
  }
  // Log("[BK] W 0x%02x %u", reg, data);
  gpio_set_scn(true);
  gpio_set_scl(false);
  // TIMER_DelayTicks(1);
  __asm volatile("nop \n"
                 "nop \n");
  gpio_set_scn(false);

  spi_write_byte(reg);
  spi_write_word(data);

  gpio_set_scn(true);
  // TIMER_DelayTicks(1);
  gpio_set_scl(true);
  // gpio_set_sda(true);
}

uint16_t BK4819_GetRegValue(RegisterSpec spec) {
  return (BK4819_ReadRegister(spec.num) >> spec.offset) & spec.mask;
}

void BK4819_SetRegValue(RegisterSpec spec, uint16_t value) {
  uint16_t reg = BK4819_ReadRegister(spec.num);
  reg &= ~(spec.mask << spec.offset);
  BK4819_WriteRegister(spec.num, reg | (value << spec.offset));
}

// ============================================================================
// Utility Functions
// ============================================================================

static inline uint16_t scale_frequency(uint16_t freq) {
  return (((uint32_t)freq * 1353245u) + (1u << 16)) >> 17; // with rounding
}

void BK4819_Idle(void) { BK4819_WriteRegister(BK4819_REG_30, 0x0000); }

void BK4819_Sleep(void) {
  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

// ============================================================================
// GPIO Control
// ============================================================================

void BK4819_ToggleGpioOut(BK4819_GPIO_PIN_t pin, bool enable) {
  const uint16_t pin_bit = 0x40U >> pin;

  if (enable) {
    gGpioOutState |= pin_bit;
  } else {
    gGpioOutState &= ~pin_bit;
  }

  BK4819_WriteRegister(BK4819_REG_33, gGpioOutState);
}

// ============================================================================
// AGC Configuration
// ============================================================================

static void configure_agc_registers(void) {
  BK4819_WriteRegister(0x12, (3u << 8) | (3u << 5) | (3u << 3) | (4u << 0));
  BK4819_WriteRegister(0x11, (2u << 8) | (3u << 5) | (3u << 3) | (3u << 0));
  BK4819_WriteRegister(0x10, (0u << 8) | (3u << 5) | (3u << 3) | (2u << 0));
  BK4819_WriteRegister(0x14, (0u << 8) | (0u << 5) | (3u << 3) | (1u << 0));
}

uint8_t BK4819_GetAttenuation() {
  uint16_t v = BK4819_ReadRegister(BK4819_REG_13);
  return (uint8_t[]){19, 16, 11, 0}[(v >> 8) & 3] +
         (uint8_t[]){24, 19, 14, 9, 6, 4, 2, 0}[(v >> 5) & 7] +
         (uint8_t[]){8, 3, 6, 0}[(v >> 3) & 3] +
         (uint8_t[]){33, 27, 21, 15, 9, 6, 3, 0}[v & 7];
}

void BK4819_SetAGC(bool useDefault, uint8_t gainIndex) {
  const bool enableAgc = (gainIndex == AUTO_GAIN_INDEX);
  uint16_t regVal = BK4819_ReadRegister(BK4819_REG_7E);

  BK4819_WriteRegister(BK4819_REG_7E,
                       (regVal & ~(1 << 15) & ~(0b111 << 12)) |
                           (!enableAgc << 15) | // AGC fix mode
                           (3u << 12) |         // AGC fix index
                           (5u << 3) |          // Default DC
                           (6u << 0));

  if (enableAgc) {
    BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
  } else {
    BK4819_WriteRegister(BK4819_REG_13, GAIN_TABLE[gainIndex].regValue);
  }

  configure_agc_registers();

  // const AgcConfig *config = useDefault ? &AGC_DEFAULT : &AGC_FAST;
  const AgcConfig *config = &AGC_DEFAULT;
  BK4819_WriteRegister(BK4819_REG_49, (config->lo << 14) | (config->high << 7) |
                                          (config->low << 0));
  BK4819_WriteRegister(BK4819_REG_7B, 0x8420); // 0x8420
}

// ============================================================================
// Filter Management
// ============================================================================

void BK4819_SelectFilterEx(Filter filter) {
  if (gSelectedFilter == filter) {
    return;
  }
  gSelectedFilter = filter;
  // Log("flt=%u", filter);
  const uint16_t PIN_BIT_VHF = 0x40U >> BK4819_GPIO4_PIN32_VHF_LNA;
  const uint16_t PIN_BIT_UHF = 0x40U >> BK4819_GPIO3_PIN31_UHF_LNA;

  if (filter == FILTER_VHF) {
    gGpioOutState |= PIN_BIT_VHF;
  } else {
    gGpioOutState &= ~PIN_BIT_VHF;
  }

  if (filter == FILTER_UHF) {
    gGpioOutState |= PIN_BIT_UHF;
  } else {
    gGpioOutState &= ~PIN_BIT_UHF;
  }

  BK4819_WriteRegister(BK4819_REG_33, gGpioOutState);
}

void BK4819_SelectFilter(uint32_t frequency) {
  Filter filter =
      (frequency < SETTINGS_GetFilterBound()) ? FILTER_VHF : FILTER_UHF;

  BK4819_SelectFilterEx(filter);
}

void BK4819_SetFilterBandwidth(BK4819_FilterBandwidth_t bw) {
  if (bw > 9)
    return;

  bw = 9 - bw; // Fix for incremental order

  static const uint8_t rf[] = {7, 5, 4, 3, 2, 1, 3, 1, 1, 0};
  static const uint8_t af[] = {4, 5, 6, 7, 0, 0, 3, 0, 2, 1};
  static const uint8_t bs[] = {2, 2, 2, 2, 2, 2, 0, 0, 1, 1};

  const uint16_t value = //
      (0u << 15)         //
      | (rf[bw] << 12)   //
      | (rf[bw] << 9)    //
      | (af[bw] << 6)    //
      | (bs[bw] << 4)    //
      | (1u << 3)        //
      | (0u << 2)        //
      | (0u << 0);       //

  // BK4819_WriteRegister(0x43, 0x347C); // AM 0b11010001111100
  // BK4819_WriteRegister(0x43, 0x3028); // FM 0b11000000101000
  BK4819_WriteRegister(BK4819_REG_43, value);
}

// ============================================================================
// Frequency Management
// ============================================================================

void BK4819_SetFrequency(uint32_t freq) {
  static uint16_t prev_low = 0;
  static uint16_t prev_high = 0;

  freq += (gSettings.freqCorrection - 127);
  // printf("f=%u\n", freq);

  uint16_t low = freq & 0xFFFF;
  uint16_t high = (freq >> 16) & 0xFFFF;

  if (low != prev_low) {
    BK4819_WriteRegister(BK4819_REG_38, low);
    prev_low = low;
  }

  if (high != prev_high) {
    BK4819_WriteRegister(BK4819_REG_39, high);
    prev_high = high;
  }
}

uint32_t BK4819_GetFrequency(void) {
  return (BK4819_ReadRegister(BK4819_REG_39) << 16) |
         BK4819_ReadRegister(BK4819_REG_38);
}

void BK4819_TuneTo(uint32_t freq, bool precise) {
  BK4819_SetFrequency(freq);
  gLastFrequency = freq;

  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);

  if (precise) {
    BK4819_WriteRegister(BK4819_REG_30, 0x0200);
  } else {
    BK4819_WriteRegister(BK4819_REG_30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
  }

  BK4819_WriteRegister(BK4819_REG_30, reg);
}

// ============================================================================
// Modulation
// ============================================================================

ModulationType BK4819_GetModulation(void) {
  uint16_t value = BK4819_ReadRegister(BK4819_REG_47) >> 8;

  for (uint8_t i = 0; i < ARRAY_SIZE(MOD_TYPE_REG47_VALUES); ++i) {
    if (MOD_TYPE_REG47_VALUES[i] == (value & 0b1111)) {
      return i;
    }
  }

  return MOD_FM;
}

void BK4819_SetAF(BK4819_AF_Type_t af) {
  BK4819_WriteRegister(BK4819_REG_47, 0x6040 | (af << 8));
}

void BK4819_SetModulation(ModulationType type) {
  if (gLastModulation == type) {
    return;
  }

  if (type == MOD_BYP) {
    BK4819_EnterBypass();
  } else if (gLastModulation == MOD_BYP) {
    BK4819_ExitBypass();
  }

  gLastModulation = type;

  const bool isSsb = (type == MOD_LSB || type == MOD_USB);
  // const bool isFm = (type == MOD_FM || type == MOD_WFM);

  BK4819_SetAF(MOD_TYPE_REG47_VALUES[type]);
  BK4819_SetRegValue(RS_AFC_DIS, isSsb);

  if (type == MOD_WFM) {
    BK4819_SetRegValue(RS_RF_FILT_BW, 7);
    BK4819_SetRegValue(RS_RF_FILT_BW_WEAK, 7);
    BK4819_SetRegValue(RS_BW_MODE, 3);
    BK4819_XtalSet(XTAL_0_13M);
  } else if (isSsb) {
    BK4819_XtalSet(XTAL_3_38_4M);
    BK4819_SetRegValue(RS_IF_F, 0);
  } else {
    BK4819_XtalSet(XTAL_2_26M);
  }

  // https://github.com/armel/uv-k1-k5v3-firmware-custom/pull/15
  uint16_t uVar1 = BK4819_ReadRegister(0x31);
  if (type == MOD_AM) {
    BK4819_WriteRegister(0x31, uVar1 | 1);
    BK4819_WriteRegister(0x42, 0x6F5C);
    // BK4819_WriteRegister(0x43, 0x347C);
    BK4819_WriteRegister(0x2A, 0x7434); // noise gate time constants
    BK4819_WriteRegister(0x2B, 0x600);  // AF RX HPF300 filter is disabled
    BK4819_WriteRegister(0x2F, 0x9990);
  } else {
    BK4819_WriteRegister(0x31, uVar1 & 0xFFFFFFFE);
    BK4819_WriteRegister(0x42, 0x6B5A);
    // BK4819_WriteRegister(0x43, 0x3028);
    BK4819_WriteRegister(0x2A, 0x7400);
    BK4819_WriteRegister(0x2B, 0);
    BK4819_WriteRegister(0x2F, 0x9890);
  }
}

// ============================================================================
// Squelch
// ============================================================================

void BK4819_SetupSquelch(SQL sq, uint8_t delayOpen, uint8_t delayClose) {
  sq.no = Clamp(sq.no, 0, 127);
  sq.nc = Clamp(sq.nc, 0, 127);

  BK4819_WriteRegister(BK4819_REG_4D, 0xA000 | sq.gc);
  BK4819_WriteRegister(BK4819_REG_4E, (1u << 14) | (delayOpen << 11) |
                                          (delayClose << 9) | sq.go);
  BK4819_WriteRegister(BK4819_REG_4F, (sq.nc << 8) | sq.no);
  BK4819_WriteRegister(BK4819_REG_78, (sq.ro << 8) | sq.rc);
}

void BK4819_Squelch(uint8_t sql, uint8_t openDelay, uint8_t closeDelay) {
  BK4819_SetupSquelch(GetSql(sql), openDelay, closeDelay);
}

void BK4819_SquelchType(SquelchType type) {
  BK4819_SetRegValue(RS_SQ_TYPE, SQUELCH_TYPE_VALUES[type]);
}

bool BK4819_IsSquelchOpen(void) {
  return (BK4819_ReadRegister(BK4819_REG_0C) >> 1) & 1;
}

// ============================================================================
// CTCSS/CDCSS
// ============================================================================

void BK4819_SetCDCSSCodeWord(uint32_t codeWord) {
  BK4819_WriteRegister(
      BK4819_REG_51,
      BK4819_REG_51_ENABLE_CxCSS | BK4819_REG_51_GPIO6_PIN2_NORMAL |
          BK4819_REG_51_TX_CDCSS_POSITIVE | BK4819_REG_51_MODE_CDCSS |
          BK4819_REG_51_CDCSS_23_BIT | BK4819_REG_51_1050HZ_NO_DETECTION |
          BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
          BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
          (51U << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

  BK4819_WriteRegister(BK4819_REG_07,
                       BK4819_REG_07_MODE_CTC1 |
                           (2775U << BK4819_REG_07_SHIFT_FREQUENCY));

  BK4819_WriteRegister(BK4819_REG_08, (codeWord >> 0) & 0xFFF);
  BK4819_WriteRegister(BK4819_REG_08, 0x8000 | ((codeWord >> 12) & 0xFFF));
}

void BK4819_SetCTCSSFrequency(uint32_t freqControlWord) {
  uint16_t config = (freqControlWord == 2625) ? 0x944A : 0x904A;

  BK4819_WriteRegister(BK4819_REG_51, config);
  BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 |
                                          (((freqControlWord * 2065) / 1000)
                                           << BK4819_REG_07_SHIFT_FREQUENCY));
}

void BK4819_SetTailDetection(uint32_t freq_10Hz) {
  BK4819_WriteRegister(BK4819_REG_07,
                       BK4819_REG_07_MODE_CTC2 |
                           ((253910 + (freq_10Hz / 2)) / freq_10Hz));
}

void BK4819_ExitSubAu(void) { BK4819_WriteRegister(BK4819_REG_51, 0x0000); }

void BK4819_EnableCDCSS(void) {
  BK4819_GenTail(0); // CTC134
  BK4819_WriteRegister(BK4819_REG_51, 0x804A);
}

void BK4819_EnableCTCSS(void) {
  BK4819_GenTail(4); // CTC55
  BK4819_WriteRegister(BK4819_REG_51, 0x904A);
}

void BK4819_GenTail(uint8_t tail) {
  switch (tail) {
  case 0: // 134.4Hz CTCSS Tail
    BK4819_WriteRegister(BK4819_REG_52, 0x828F);
    break;
  case 1: // 120° phase shift
    BK4819_WriteRegister(BK4819_REG_52, 0xA28F);
    break;
  case 2: // 180° phase shift
    BK4819_WriteRegister(BK4819_REG_52, 0xC28F);
    break;
  case 3: // 240° phase shift
    BK4819_WriteRegister(BK4819_REG_52, 0xE28F);
    break;
  case 4: // 55Hz tone freq
    BK4819_WriteRegister(BK4819_REG_07, 0x046f);
    break;
  }
}

BK4819_CssScanResult_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq,
                                                 uint16_t *pCtcssFreq) {
  uint16_t high = BK4819_ReadRegister(BK4819_REG_69);

  if ((high & 0x8000) == 0) {
    uint16_t low = BK4819_ReadRegister(BK4819_REG_6A);
    *pCdcssFreq = ((high & 0xFFF) << 12) | (low & 0xFFF);
    return BK4819_CSS_RESULT_CDCSS;
  }

  uint16_t low = BK4819_ReadRegister(BK4819_REG_68);
  if ((low & 0x8000) == 0) {
    *pCtcssFreq = ((low & 0x1FFF) * 4843) / 10000;
    return BK4819_CSS_RESULT_CTCSS;
  }

  return BK4819_CSS_RESULT_NOT_FOUND;
}

uint8_t BK4819_GetCDCSSCodeType(void) {
  return (BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3;
}

uint8_t BK4819_GetCTCType(void) {
  return (BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3;
}

// ============================================================================
// DTMF
// ============================================================================

static void write_dtmf_tone(uint16_t tone1, uint16_t tone2) {
  BK4819_WriteRegister(BK4819_REG_71, tone1);
  BK4819_WriteRegister(BK4819_REG_72, tone2);
}

void BK4819_PlayDTMF(char code) {
  const struct {
    char c;
    uint16_t tone1;
    uint16_t tone2;
  } dtmf_map[] = {
      {'0', 0x25F3, 0x35E1}, {'1', 0x1C1C, 0x30C2}, {'2', 0x1C1C, 0x35E1},
      {'3', 0x1C1C, 0x3B91}, {'4', 0x1F0E, 0x30C2}, {'5', 0x1F0E, 0x35E1},
      {'6', 0x1F0E, 0x3B91}, {'7', 0x225C, 0x30C2}, {'8', 0x225c, 0x35E1},
      {'9', 0x225C, 0x3B91}, {'A', 0x1C1C, 0x41DC}, {'B', 0x1F0E, 0x41DC},
      {'C', 0x225C, 0x41DC}, {'D', 0x25F3, 0x41DC}, {'*', 0x25F3, 0x30C2},
      {'#', 0x25F3, 0x3B91},
  };

  for (size_t i = 0; i < ARRAY_SIZE(dtmf_map); i++) {
    if (dtmf_map[i].c == code) {
      write_dtmf_tone(dtmf_map[i].tone1, dtmf_map[i].tone2);
      return;
    }
  }
}

void BK4819_EnableDTMF(void) {
  BK4819_WriteRegister(BK4819_REG_21, 0x06D8);
  BK4819_WriteRegister(BK4819_REG_24,
                       (1U << BK4819_REG_24_SHIFT_UNKNOWN_15) |
                           (24 << BK4819_REG_24_SHIFT_THRESHOLD) |
                           (1U << BK4819_REG_24_SHIFT_UNKNOWN_6) |
                           BK4819_REG_24_ENABLE | BK4819_REG_24_SELECT_DTMF |
                           (14U << BK4819_REG_24_SHIFT_MAX_SYMBOLS));
}

void BK4819_DisableDTMF(void) { BK4819_WriteRegister(BK4819_REG_24, 0); }

void BK4819_EnterDTMF_TX(bool localLoopback) {
  BK4819_EnableDTMF();
  BK4819_EnterTxMute();
  BK4819_SetAF(localLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
  BK4819_WriteRegister(BK4819_REG_70,
                       BK4819_REG_70_MASK_ENABLE_TONE1 |
                           (83 << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN) |
                           BK4819_REG_70_MASK_ENABLE_TONE2 |
                           (83 << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN));
  BK4819_EnableTXLink();
}

void BK4819_ExitDTMF_TX(bool keep) {
  BK4819_EnterTxMute();
  BK4819_SetAF(BK4819_AF_MUTE);
  BK4819_WriteRegister(BK4819_REG_70, 0x0000);
  BK4819_DisableDTMF();
  BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
  if (!keep) {
    BK4819_ExitTxMute();
  }
}

void BK4819_PlayDTMFString(const char *string, bool delayFirst,
                           uint16_t firstPersist, uint16_t hashPersist,
                           uint16_t codePersist, uint16_t codeInterval) {
  for (uint8_t i = 0; string[i]; i++) {
    BK4819_PlayDTMF(string[i]);
    BK4819_ExitTxMute();

    uint16_t delay;
    if (delayFirst && i == 0) {
      delay = firstPersist;
    } else if (string[i] == '*' || string[i] == '#') {
      delay = hashPersist;
    } else {
      delay = codePersist;
    }

    SYS_DelayMs(delay);
    BK4819_EnterTxMute();
    SYS_DelayMs(codeInterval);
  }
}

void BK4819_PlayDTMFEx(bool localLoopback, char code) {
  BK4819_EnableDTMF();
  BK4819_EnterTxMute();
  BK4819_SetAF(localLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
  BK4819_WriteRegister(BK4819_REG_70, 0xD3D3);
  BK4819_EnableTXLink();
  SYS_DelayMs(50);
  BK4819_PlayDTMF(code);
  BK4819_ExitTxMute();
}

uint8_t BK4819_GetDTMF_5TONE_Code(void) {
  return (BK4819_ReadRegister(BK4819_REG_0B) >> 8) & 0x0F;
}

// ============================================================================
// Tone Generation
// ============================================================================

void BK4819_SetToneFrequency(uint16_t freq) {
  BK4819_WriteRegister(BK4819_REG_71, scale_frequency(freq));
}

void BK4819_SetTone2Frequency(uint16_t freq) {
  BK4819_WriteRegister(BK4819_REG_72, scale_frequency(freq));
}

void BK4819_PlayTone(uint16_t frequency, bool tuningGainSwitch) {
  BK4819_EnterTxMute();
  BK4819_SetAF(BK4819_AF_BEEP);

  uint8_t gain = tuningGainSwitch ? 28 : 96;
  uint16_t toneCfg = BK4819_REG_70_ENABLE_TONE1 |
                     (gain << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN);

  BK4819_WriteRegister(BK4819_REG_70, toneCfg);
  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_30, BK4819_REG_30_ENABLE_AF_DAC |
                                          BK4819_REG_30_ENABLE_DISC_MODE |
                                          BK4819_REG_30_ENABLE_TX_DSP);

  BK4819_SetToneFrequency(frequency);
}

void BK4819_TransmitTone(uint32_t frequency) {
  BK4819_EnterTxMute();
  BK4819_WriteRegister(BK4819_REG_70,
                       BK4819_REG_70_MASK_ENABLE_TONE1 |
                           (56 << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
  BK4819_SetToneFrequency(frequency);
  BK4819_SetAF(gSettings.toneLocal ? BK4819_AF_BEEP : BK4819_AF_MUTE);
  BK4819_EnableTXLink();
  BK4819_ExitTxMute();
}

void BK4819_PlayRogerTiny(void) {
  const uint16_t sequence[] = {1250, 20, 0, 10, 1500, 20, 0, 0};
  BK4819_PlaySequence(sequence);
}

void BK4819_PlaySequence(const uint16_t *sequence) {
  bool initialTone = true;

  for (uint8_t i = 0; i < 255; i += 2) {
    uint16_t note = sequence[i];
    uint16_t duration = sequence[i + 1];

    if (!note && !duration) {
      break;
    }

    if (initialTone) {
      initialTone = false;
      BK4819_TransmitTone(note);
      if (gSettings.toneLocal) {
        SYS_DelayMs(10);
        AUDIO_ToggleSpeaker(true);
      }
    } else {
      BK4819_SetToneFrequency(note);
      BK4819_ExitTxMute();
    }

    if (note && !duration) {
      return;
    }

    SYS_DelayMs(duration);
  }

  if (gSettings.toneLocal) {
    AUDIO_ToggleSpeaker(false);
    SYS_DelayMs(10);
  }
  BK4819_EnterTxMute();
}

// ============================================================================
// TX/RX Control
// ============================================================================

void BK4819_EnterTxMute(void) { BK4819_WriteRegister(BK4819_REG_50, 0xBB20); }

void BK4819_ExitTxMute(void) { BK4819_WriteRegister(BK4819_REG_50, 0x3B20); }

void BK4819_RX_TurnOn(void) {
  BK4819_WriteRegister(BK4819_REG_36, 0x0000);
  BK4819_WriteRegister(BK4819_REG_37, 0x1F0F);
  BK4819_WriteRegister(BK4819_REG_30, 0x0200);

  BK4819_WriteRegister(
      BK4819_REG_30,
      BK4819_REG_30_ENABLE_VCO_CALIB | BK4819_REG_30_DISABLE_UNKNOWN |
          BK4819_REG_30_ENABLE_RX_LINK | BK4819_REG_30_ENABLE_AF_DAC |
          BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_PLL_VCO |
          BK4819_REG_30_DISABLE_PA_GAIN | BK4819_REG_30_DISABLE_MIC_ADC |
          BK4819_REG_30_DISABLE_TX_DSP | BK4819_REG_30_ENABLE_RX_DSP);
}

void BK4819_EnableTXLink(void) {
  BK4819_WriteRegister(
      BK4819_REG_30,
      BK4819_REG_30_ENABLE_VCO_CALIB | BK4819_REG_30_ENABLE_UNKNOWN |
          BK4819_REG_30_DISABLE_RX_LINK | BK4819_REG_30_ENABLE_AF_DAC |
          BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_PLL_VCO |
          BK4819_REG_30_ENABLE_PA_GAIN | BK4819_REG_30_DISABLE_MIC_ADC |
          BK4819_REG_30_ENABLE_TX_DSP | BK4819_REG_30_DISABLE_RX_DSP);
}

void BK4819_PrepareTransmit(void) {
  BK4819_ExitBypass();
  BK4819_ExitTxMute();
  BK4819_TxOn_Beep();
}

void BK4819_TxOn_Beep(void) {
  BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
  BK4819_WriteRegister(BK4819_REG_52, 0x028F);
  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
}

void BK4819_TurnsOffTones_TurnsOnRX(void) {
  BK4819_WriteRegister(BK4819_REG_70, 0);
  BK4819_SetAF(BK4819_AF_MUTE);
  BK4819_ExitTxMute();
  BK4819_Idle();
  BK4819_WriteRegister(
      BK4819_REG_30,
      BK4819_REG_30_ENABLE_VCO_CALIB | BK4819_REG_30_ENABLE_RX_LINK |
          BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE |
          BK4819_REG_30_ENABLE_PLL_VCO | BK4819_REG_30_ENABLE_RX_DSP);
}

void BK4819_SetupPowerAmplifier(uint8_t bias, uint32_t frequency) {
  uint8_t gain = (frequency < VHF_UHF_BOUND2) ? 0x08 : 0x22;
  BK4819_WriteRegister(BK4819_REG_36, (bias << 8) | 0x80U | gain);
}

// ============================================================================
// Bypass Mode
// ============================================================================

void BK4819_EnterBypass(void) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_7E);
  BK4819_WriteRegister(BK4819_REG_7E, reg & ~(0b111 << 3) & ~(0b111 << 0));
}

void BK4819_ExitBypass(void) {
  BK4819_SetAF(BK4819_AF_MUTE);
  BK4819_WriteRegister(BK4819_REG_7E, 0x302E);
}

// ============================================================================
// VOX
// ============================================================================

void BK4819_EnableVox(uint16_t enableThreshold, uint16_t disableThreshold) {
  uint16_t reg31 = BK4819_ReadRegister(BK4819_REG_31);

  BK4819_WriteRegister(BK4819_REG_46, 0xA000 | (enableThreshold & 0x07FF));
  BK4819_WriteRegister(BK4819_REG_79, 0x1800 | (disableThreshold & 0x07FF));
  BK4819_WriteRegister(BK4819_REG_7A, 0x289A);    // 640ms disable delay
  BK4819_WriteRegister(BK4819_REG_31, reg31 | 4); // Enable VOX
}

void BK4819_DisableVox(void) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_31);
  BK4819_WriteRegister(BK4819_REG_31, reg & 0xFFFB);
}

void BK4819_GetVoxAmp(uint16_t *result) {
  *result = BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF;
}

// ============================================================================
// Scrambler
// ============================================================================

void BK4819_EnableScramble(uint8_t type) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_31);
  BK4819_WriteRegister(BK4819_REG_31, reg | 2);
  BK4819_WriteRegister(BK4819_REG_71, (type * 0x0408) + 0x68DC);
}

void BK4819_DisableScramble(void) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_31);
  BK4819_WriteRegister(BK4819_REG_31, reg & 0xFFFD);
}

void BK4819_SetScrambler(uint8_t type) {
  if (type) {
    BK4819_EnableScramble(type);
  } else {
    BK4819_DisableScramble();
  }
}

// ============================================================================
// Crystal Oscillator
// ============================================================================

XtalMode BK4819_XtalGet(void) {
  return (XtalMode)((BK4819_ReadRegister(0x3C) >> 6) & 0b11);
}

void BK4819_XtalSet(XtalMode mode) {
  uint16_t ifset = 0x2AAB;
  uint16_t xtal = 20360;

  switch (mode) {
  case XTAL_0_13M:
    xtal = 20232;
    ifset = 0x3555;
    break;
  case XTAL_1_19_2M:
    xtal = 20296;
    ifset = 0x2E39;
    break;
  case XTAL_2_26M:
    // Use defaults
    break;
  case XTAL_3_38_4M:
    xtal = 20424;
    ifset = 0x271C;
    break;
  }

  BK4819_WriteRegister(0x3C, xtal);
  BK4819_WriteRegister(0x3D, ifset);
}

// ============================================================================
// AFC (Automatic Frequency Control)
// ============================================================================

/**
 * @param disable
 * @param range 0..7
 * @param speed 9..63
 */
void _BK4819_SetAFC(bool disable, uint8_t range, uint8_t speed) {
  BK4819_WriteRegister(0x73,
                       0x4005 | (range << 11) | (speed << 5) | (disable << 4));
}

#define BK4819_REG_73 0x73
#define BK4819_REG_73_DISABLE (1 << 4)
#define BK4819_REG_73_LEVEL_MASK (0xF << 11) // Mask for bits 14:11
#define BK4819_REG_73_DEFAULT_LEVEL 7        // Default for disable mode

/**
 * Set AFC level for BK4819.
 * @param level 0 = off (disable AFC), 1..8 = range
 */
void BK4819_SetAFC(uint8_t level) {
  if (level > 8) {
    level = 8;
  }

  uint16_t reg_val = BK4819_ReadRegister(BK4819_REG_73);

  reg_val &= ~(BK4819_REG_73_LEVEL_MASK | BK4819_REG_73_DISABLE);

  uint8_t afc_level;
  if (level == 0) {
    // Disable AFC with default level
    afc_level = BK4819_REG_73_DEFAULT_LEVEL;
    reg_val |= BK4819_REG_73_DISABLE;
  } else {
    // Enable AFC (disable bit remains 0) with calculated level
    afc_level = 8 - level;
  }

  reg_val |= (afc_level << 11);

  BK4819_WriteRegister(BK4819_REG_73, reg_val);
}

uint8_t BK4819_GetAFC(void) {
  uint16_t afc = BK4819_ReadRegister(0x73);

  if ((afc >> 4) & 1) {
    return 0;
  }

  return 8 - ((afc >> 11) & 0b111);
}

/**
 * Set AFC speed for BK4819.
 * @param level 0(slow)..63(fast)
 */
void BK4819_SetAFCSpeed(uint8_t speed) {
  if (speed > 63) {
    speed = 63;
  }

  uint16_t reg_val = BK4819_ReadRegister(BK4819_REG_73);

  reg_val &= ~(63 << 5);
  reg_val |= ((63 - speed) << 5);

  BK4819_WriteRegister(BK4819_REG_73, reg_val);
}

uint8_t BK4819_GetAFCSpeed(void) {
  return 63 - ((BK4819_ReadRegister(0x73) >> 5) & 63);
}

// ============================================================================
// RSSI and Signal Measurements
// ============================================================================

uint8_t BK4819_GetLnaPeakRSSI(void) { return BK4819_ReadRegister(0x62) & 0xFF; }

uint8_t BK4819_GetAgcRSSI(void) {
  return (BK4819_ReadRegister(0x62) >> 8) & 0xFF;
}

uint16_t BK4819_GetRSSI(void) {
  return BK4819_ReadRegister(BK4819_REG_67) & 0x1FF;
}

uint8_t BK4819_GetNoise(void) {
  return BK4819_ReadRegister(BK4819_REG_65) & 0x7F;
}

uint8_t BK4819_GetGlitch(void) {
  return BK4819_ReadRegister(BK4819_REG_63) & 0xFF;
}

uint8_t BK4819_GetAfTxRx(void) {
  return BK4819_ReadRegister(BK4819_REG_6F) & 0x3F;
}

uint8_t BK4819_GetSignalPower(void) {
  return (BK4819_ReadRegister(0x7E) >> 6) & 0b111111;
}

int32_t BK4819_GetAFCValue() {
  int16_t signedAfc = (int16_t)BK4819_ReadRegister(0x6D);
  // * 3.3(3)
  return (int32_t)((((int64_t)signedAfc * 0xAAAAAAABLL) + 0x55555555LL) >> 33);
}

uint8_t BK4819_GetSNR(void) { return BK4819_ReadRegister(0x61) & 0xFF; }

uint16_t BK4819_GetVoiceAmplitude(void) { return BK4819_ReadRegister(0x64); }

// ============================================================================
// Frequency Scanning
// ============================================================================

bool BK4819_GetFrequencyScanResult(uint32_t *frequency) {
  uint16_t high = BK4819_ReadRegister(BK4819_REG_0D);
  bool finished = (high & 0x8000) == 0;

  if (finished) {
    uint16_t low = BK4819_ReadRegister(BK4819_REG_0E);
    *frequency = (uint32_t)((high & 0x7FF) << 16) | low;
  }

  return finished;
}

void BK4819_EnableFrequencyScan(void) {
  BK4819_WriteRegister(BK4819_REG_32, 0x0245);
}

void BK4819_EnableFrequencyScanEx(FreqScanTime time) {
  BK4819_WriteRegister(BK4819_REG_32, 0x0245 | (time << 14));
}

void BK4819_EnableFrequencyScanEx2(FreqScanTime time, uint16_t hz) {
  BK4819_WriteRegister(BK4819_REG_32, (time << 14) | (hz << 1) | true);
}

void BK4819_DisableFrequencyScan(void) {
  BK4819_WriteRegister(BK4819_REG_32, 0x0244);
}

void BK4819_StopScan(void) {
  BK4819_DisableFrequencyScan();
  BK4819_Idle();
}

// ============================================================================
// FSK (Frequency Shift Keying)
// ============================================================================

void BK4819_ResetFSK(void) {
  BK4819_WriteRegister(BK4819_REG_3F, 0x0000);
  BK4819_WriteRegister(BK4819_REG_59, 0x0068);
  SYS_DelayMs(30);
  BK4819_Idle();
}

void BK4819_FskClearFifo(void) {
  const uint16_t fskReg59 = BK4819_ReadRegister(BK4819_REG_59);
  BK4819_WriteRegister(BK4819_REG_59, (1u << 15) | (1u << 14) | fskReg59);
}

void BK4819_FskEnableRx(void) {
  const uint16_t fskReg59 = BK4819_ReadRegister(BK4819_REG_59);
  BK4819_WriteRegister(BK4819_REG_59, (1u << 12) | fskReg59);
}

void BK4819_FskEnableTx(void) {
  const uint16_t fskReg59 = BK4819_ReadRegister(BK4819_REG_59);
  BK4819_WriteRegister(BK4819_REG_59, (1u << 11) | fskReg59);
}

// ============================================================================
// Audio Control
// ============================================================================

void BK4819_ToggleAFBit(bool enable) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  reg &= ~(1 << 8);
  if (enable) {
    reg |= 1 << 8;
  }
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

void BK4819_ToggleAFDAC(bool enable) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  reg &= ~BK4819_REG_30_ENABLE_AF_DAC;
  if (enable) {
    reg |= BK4819_REG_30_ENABLE_AF_DAC;
  }
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

void BK4819_Enable_AfDac_DiscMode_TxDsp(void) {
  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

// ============================================================================
// Initialization
// ============================================================================

static void initialize_gpio_pins(void) {
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
}

static void initialize_dtmf_coefficients(void) {
  for (uint8_t i = 0; i < ARRAY_SIZE(DTMF_COEFFS); ++i) {
    BK4819_WriteRegister(0x09, (i << 12) | DTMF_COEFFS[i]);
  }
}

static void initialize_registers(void) {
  // soft reset
  BK4819_WriteRegister(BK4819_REG_00, 0x8000);
  BK4819_WriteRegister(BK4819_REG_00, 0x0000);
  // power up rf
  BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);

  gGpioOutState = 0x9000;
  BK4819_WriteRegister(BK4819_REG_33, gGpioOutState);

  // PA
  BK4819_SetupPowerAmplifier(0, 0);

  BK4819_WriteRegister(BK4819_REG_1E, 0x4c58); // for some revisions
  BK4819_WriteRegister(BK4819_REG_1F, 0x5454); // VCO, PLL

  BK4819_SetAGC(true, AUTO_GAIN_INDEX);

  BK4819_WriteRegister(BK4819_REG_3F, 0); // interrupts

  BK4819_WriteRegister(BK4819_REG_3E, 0xA037); // band sel tres
}

static void initialize_audio(void) {
  BK4819_WriteRegister(BK4819_REG_48,
                       (11u << 12) |   // Reserved
                           (0 << 10) | // AF Rx ATT-1: 0..-18dB (-6dB/step)
                           (58 << 4) | // AF Rx Gain-2 (-26..5.5 dB, 0.5dB/step)
                           (8 << 0)    // AF DAC Gain (0..15max 2dB/step)
  );
}

static void wait_for_crystal_stabilization(void) {
  while (BK4819_ReadRegister(BK4819_REG_0C) & 1U) {
    BK4819_WriteRegister(BK4819_REG_02, 0);
    SYS_DelayMs(1);
  }
}

static void configure_microphone_and_tx(void) {
  BK4819_WriteRegister(BK4819_REG_19, 0x1041);                 // MIC PGA
  BK4819_WriteRegister(BK4819_REG_7D, 0xE940 | gSettings.mic); // MIC sens
  BK4819_WriteRegister(0x74, 0xAF1F); // 3kHz response TX
}

static void configure_receiver(void) {
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
}

static bool isInitialized = false;

void BK4819_Init(void) {
  if (isInitialized) {
    return;
  }
  gSelectedFilter = 255;
  gLastFrequency = 0;
  gLastModulation = 255;
  initialize_gpio_pins();
  initialize_registers();
  initialize_dtmf_coefficients();
  initialize_audio();
  wait_for_crystal_stabilization();
  configure_microphone_and_tx();
  configure_receiver();

  BK4819_DisableDTMF();

  // Set deviation
  BK4819_WriteRegister(0x40, (BK4819_ReadRegister(0x40) & ~(0x7FF)) |
                                 (gSettings.deviation * 10) | (1 << 12));
  isInitialized = true;
}
