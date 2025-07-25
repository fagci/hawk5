#include "bk4819.h"
#include "../driver/gpio.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../driver/uart.h"
#include "../inc/dp32g030/gpio.h"
#include "../inc/dp32g030/portcon.h"
#include "../misc.h"
#include "../settings.h"
#include "audio.h"
#include "bk4819-regs.h"

static uint16_t gBK4819_GpioOutState;
static Filter selectedFilter = FILTER_OFF;

static const uint16_t modTypeReg47Values[] = {
    [MOD_FM] = BK4819_AF_FM,      //
    [MOD_AM] = BK4819_AF_AM,      //
    [MOD_LSB] = BK4819_AF_USB,    //
    [MOD_USB] = BK4819_AF_USB,    //
    [MOD_BYP] = BK4819_AF_BYPASS, //
    [MOD_RAW] = BK4819_AF_RAW,    //
    [MOD_WFM] = BK4819_AF_FM,     //
};
static const uint8_t squelchTypeValues[4] = {0x88, 0xAA, 0xCC, 0xFF};
static const uint8_t DTMF_COEFFS[] = {111, 107, 103, 98, 80,  71,  58,  44,
                                      65,  55,  37,  23, 228, 203, 181, 159};

const Gain gainTable[32] = {
    {0x10, 90},  {0x1, 88},   {0x9, 87},   {0x2, 83},   {0xA, 81},
    {0x12, 79},  {0x2A, 77},  {0x32, 75},  {0x3A, 70},  {0x20B, 68},
    {0x213, 64}, {0x21B, 62}, {0x214, 59}, {0x21C, 56}, {0x22D, 52},
    {0x23C, 50}, {0x23D, 48}, {0x255, 44}, {0x25D, 42}, {0x275, 39},
    {0x295, 33}, {0x2B6, 31}, {0x354, 28}, {0x36C, 23}, {0x38C, 20},
    {0x38D, 17}, {0x3B5, 13}, {0x3B6, 9},  {0x3D6, 8},  {0x3BF, 3},
    {0x3DF, 2},  {0x3FF, 0},
};

inline uint16_t scale_freq(const uint16_t freq) {
  return (((uint32_t)freq * 1353245u) + (1u << 16)) >> 17; // with rounding
}

void BK4819_Idle(void) { BK4819_WriteRegister(BK4819_REG_30, 0x0000); }

void BK4819_Init(void) {
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

  BK4819_WriteRegister(BK4819_REG_00, 0x8000);
  BK4819_WriteRegister(BK4819_REG_00, 0x0000);
  BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
  BK4819_WriteRegister(BK4819_REG_36, 0x0022);
  BK4819_SetAGC(true, 0);
  BK4819_WriteRegister(BK4819_REG_19, 0x1041);
  BK4819_WriteRegister(BK4819_REG_7D, 0xE94F);

  for (uint8_t i = 0; i < ARRAY_SIZE(DTMF_COEFFS); ++i) {
    BK4819_WriteRegister(0x09, (i << 12) | DTMF_COEFFS[i]);
  }
  BK4819_WriteRegister(BK4819_REG_1E, 0x4c58);
  BK4819_WriteRegister(BK4819_REG_1F, 0x5454);
  BK4819_WriteRegister(BK4819_REG_3E, 0xA037);
  gBK4819_GpioOutState = 0x9000;
  BK4819_WriteRegister(BK4819_REG_33, 0x9000);
  BK4819_WriteRegister(BK4819_REG_3F, 0);

  // ---
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
  BK4819_SetupPowerAmplifier(0, 0); // 0 is default, but...

  while (BK4819_ReadRegister(BK4819_REG_0C) & 1U) {
    BK4819_WriteRegister(BK4819_REG_02, 0);
    SYS_DelayMs(1);
  }
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  BK4819_WriteRegister(BK4819_REG_7D, 0xE94F | 10); // mic
  BK4819_SetRegValue(RS_MIC, gSettings.mic);
  // TX
  // BK4819_WriteRegister(0x44, 38888);  // 300 resp TX
  BK4819_WriteRegister(0x74, 0xAF1F); // 3k resp TX

  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
  BK4819_WriteRegister(
      BK4819_REG_48,
      (11u << 12) |   // ??? .. 0 ~ 15, doesn't seem to make any difference
          (0 << 10) | // AF Rx Gain-1 00:0dB 01:-6dB 10:-12dB 11:-18dB
          (58 << 4) | // AF Rx Gain-2 AF RX Gain2 (-26 dB ~ 5.5 dB): 0x00: Mute
          (8 << 0));  // AF DAC Gain (after Gain-1 and Gain-2) 1111 - max

  BK4819_DisableDTMF();

  BK4819_WriteRegister(0x40, (BK4819_ReadRegister(0x40) & ~(0x7FF)) |
                                 (gSettings.deviation * 10) | (1 << 12));
}

void BK4819_WriteU8(uint8_t Data) {
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  for (uint8_t i = 0; i < 8; i++) {
    if ((Data & 0x80U) == 0) {
      GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    } else {
      GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    }
    SYSTICK_Delay250ns(1);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    SYSTICK_Delay250ns(1);
    Data <<= 1;
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    SYSTICK_Delay250ns(1);
  }
}

static uint16_t BK4819_ReadU16(void) {

  PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) |
                     PORTCON_PORTC_IE_C2_BITS_ENABLE;
  GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;
  SYSTICK_Delay250ns(1);

  uint16_t v = 0;
  for (uint8_t i = 0; i < 16; i++) {
    v <<= 1;
    v |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    SYSTICK_Delay250ns(1);
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    SYSTICK_Delay250ns(1);
  }
  PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) |
                     PORTCON_PORTC_IE_C2_BITS_DISABLE;
  GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

  return v;
}

void BK4819_WriteU16(uint16_t Data) {
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  for (uint8_t i = 0; i < 16; i++) {
    if ((Data & 0x8000U) == 0U) {
      GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    } else {
      GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    }
    SYSTICK_Delay250ns(1);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    Data <<= 1;
    SYSTICK_Delay250ns(1);
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    SYSTICK_Delay250ns(1);
  }
}

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t Register) {
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  SYSTICK_Delay250ns(1);
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

  BK4819_WriteU8(Register | 0x80);

  uint16_t v = BK4819_ReadU16();

  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  SYSTICK_Delay250ns(1);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
  return v;
}

void BK4819_WriteRegister(BK4819_REGISTER_t Register, uint16_t Data) {
  // Log("  BK W 0x%02x: 0x%04x", Register, Data);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  SYSTICK_Delay250ns(1);
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  BK4819_WriteU8(Register);
  BK4819_WriteU16(Data);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
  SYSTICK_Delay250ns(1);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
}

void BK4819_SetAGC(bool useDefault, uint8_t gainIndex) {
  const bool enableAgc = gainIndex == AUTO_GAIN_INDEX;
  uint16_t regVal = BK4819_ReadRegister(BK4819_REG_7E);

  BK4819_WriteRegister(BK4819_REG_7E, (regVal & ~(1 << 15) & ~(0b111 << 12)) |
                                          (!enableAgc << 15) // 0  AGC fix mode
                                          | (3u << 12)       // 3  AGC fix index
                                          | (5u << 3) | (6u << 0) // def DC
  );

  if (gainIndex == AUTO_GAIN_INDEX) {
    BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
    // BK4819_WriteRegister(BK4819_REG_13, 0x0295);
  } else {
    BK4819_WriteRegister(BK4819_REG_13, gainTable[gainIndex].regValue);
  }
  /* BK4819_WriteRegister(BK4819_REG_12, 0x037C);
  BK4819_WriteRegister(BK4819_REG_11, 0x027B);
  BK4819_WriteRegister(BK4819_REG_10, 0x007A); */

  BK4819_WriteRegister(
      0x12, (3u << 8) | (3u << 5) | (3u << 3) |
                (4u << 0)); // 000000 11 011 11 100  0x037C =  3 3 3 4
  BK4819_WriteRegister(
      0x11, (2u << 8) | (3u << 5) | (3u << 3) |
                (3u << 0)); // 000000 10 011 11 011  0x027B =  2 3 3 3
  BK4819_WriteRegister(
      0x10, (0u << 8) | (3u << 5) | (3u << 3) |
                (2u << 0)); // 000000 00 011 11 010  0x007A =  0 3 3 2
  BK4819_WriteRegister(
      0x14, (0u << 8) | (0u << 5) | (3u << 3) |
                (1u << 0)); // 000000 00 000 11 000  0x0019 =  0 0 3 1

  uint8_t Lo = 0;    // 0-1 - auto, 2 - low, 3 high
  uint8_t low = 56;  // 1dB / LSB 56
  uint8_t high = 84; // 1dB / LSB 84

  BK4819_WriteRegister(
      0x14, (0u << 8) | (0u << 5) | (3u << 3) |
                (1u << 0)); // 000000 00 000 11 000  0x0019 =  0 0 3 1
  if (useDefault) {
    // BK4819_WriteRegister(BK4819_REG_14, 0x0019);
  } else {
    // BK4819_WriteRegister(BK4819_REG_14, 0x0000);
    // slow 25 45
    // fast 15 50
    low = 20;
    high = 50;
  }
  BK4819_WriteRegister(BK4819_REG_49, (Lo << 14) | (high << 7) | (low << 0));
  BK4819_WriteRegister(BK4819_REG_7B, 0x8420);
}

void BK4819_ToggleGpioOut(BK4819_GPIO_PIN_t Pin, bool bSet) {
  const uint16_t PIN_BIT = 0x40U >> Pin;
  if (bSet) {
    gBK4819_GpioOutState |= PIN_BIT;
  } else {
    gBK4819_GpioOutState &= ~PIN_BIT;
  }

  BK4819_WriteRegister(BK4819_REG_33, gBK4819_GpioOutState);
}

void BK4819_SetCDCSSCodeWord(uint32_t CodeWord) {
  BK4819_WriteRegister(
      BK4819_REG_51,
      0 | BK4819_REG_51_ENABLE_CxCSS | BK4819_REG_51_GPIO6_PIN2_NORMAL |
          BK4819_REG_51_TX_CDCSS_POSITIVE | BK4819_REG_51_MODE_CDCSS |
          BK4819_REG_51_CDCSS_23_BIT | BK4819_REG_51_1050HZ_NO_DETECTION |
          BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
          BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
          (51U << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

  // CTC1 Frequency Control Word = 2775
  BK4819_WriteRegister(BK4819_REG_07,
                       0 | BK4819_REG_07_MODE_CTC1 |
                           (2775U << BK4819_REG_07_SHIFT_FREQUENCY));

  // Set the code word
  BK4819_WriteRegister(BK4819_REG_08, (CodeWord >> 0) & 0xFFF);
  BK4819_WriteRegister(BK4819_REG_08, 0x8000 | ((CodeWord >> 12) & 0xFFF));
}

void BK4819_SetCTCSSFrequency(uint32_t FreqControlWord) {
  uint16_t Config;

  if (FreqControlWord == 2625) { // Enables 1050Hz detection mode
    // Enable TxCTCSS
    // CTCSS Mode
    // 1050/4 Detect Enable
    // Enable Auto CDCSS Bw Mode
    // Enable Auto CTCSS Bw Mode
    // CTCSS/CDCSS Tx Gain1 Tuning = 74
    Config = 0x944A;
  } else {
    // Enable TxCTCSS
    // CTCSS Mode
    // Enable Auto CDCSS Bw Mode
    // Enable Auto CTCSS Bw Mode
    // CTCSS/CDCSS Tx Gain1 Tuning = 74
    Config = 0x904A;
  }
  BK4819_WriteRegister(BK4819_REG_51, Config);
  // CTC1 Frequency Control Word
  BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 |
                                          ((FreqControlWord * 2065) / 1000)
                                              << BK4819_REG_07_SHIFT_FREQUENCY);
}

void BK4819_SetTailDetection(const uint32_t freq_10Hz) {
  BK4819_WriteRegister(BK4819_REG_07,
                       BK4819_REG_07_MODE_CTC2 | ((253910 + (freq_10Hz / 2)) /
                                                  freq_10Hz)); // with rounding
}

void BK4819_EnableVox(uint16_t VoxEnableThreshold,
                      uint16_t VoxDisableThreshold) {
  // VOX Algorithm
  // if(voxamp>VoxEnableThreshold)       VOX = 1;
  // else if(voxamp<VoxDisableThreshold) (After Delay) VOX = 0;
  uint16_t REG_31_Value;

  REG_31_Value = BK4819_ReadRegister(BK4819_REG_31);
  // 0xA000 is undocumented?
  BK4819_WriteRegister(BK4819_REG_46, 0xA000 | (VoxEnableThreshold & 0x07FF));
  // 0x1800 is undocumented?
  BK4819_WriteRegister(BK4819_REG_79, 0x1800 | (VoxDisableThreshold & 0x07FF));
  // Bottom 12 bits are undocumented, 15:12 vox disable delay *128ms
  BK4819_WriteRegister(BK4819_REG_7A,
                       0x289A); // vox disable delay = 128*5 = 640ms
  // Enable VOX
  BK4819_WriteRegister(BK4819_REG_31, REG_31_Value | 4); // bit 2 - VOX Enable
}

void BK4819_SetFilterBandwidth(BK4819_FilterBandwidth_t bw) {
  if (bw > 9)
    return;

  bw = 9 - bw; // fix for incremental order

  // REG_43
  // <15>    0 ???
  //

  static const uint8_t rf[] = {7, 5, 4, 3, 2, 1, 3, 1, 1, 0};

  // <14:12> 4 RF filter bandwidth
  //         0 = 1.7  KHz
  //         1 = 2.0  KHz
  //         2 = 2.5  KHz
  //         3 = 3.0  KHz *W
  //         4 = 3.75 KHz *N
  //         5 = 4.0  KHz
  //         6 = 4.25 KHz
  //         7 = 4.5  KHz
  // if <5> == 1, RF filter bandwidth * 2

  // static const uint8_t wb[] = {6, 4, 3, 2, 2, 1, 2, 1, 0, 0};

  // <11:9>  0 RF filter bandwidth when signal is weak
  //         0 = 1.7  KHz *WN
  //         1 = 2.0  KHz
  //         2 = 2.5  KHz
  //         3 = 3.0  KHz
  //         4 = 3.75 KHz
  //         5 = 4.0  KHz
  //         6 = 4.25 KHz
  //         7 = 4.5  KHz
  // if <5> == 1, RF filter bandwidth * 2

  static const uint8_t af[] = {4, 5, 6, 7, 0, 0, 3, 0, 2, 1};

  // <8:6>   1 AFTxLPF2 filter Band Width
  //         1 = 2.5  KHz (for 12.5k channel space) *N
  //         2 = 2.75 KHz
  //         0 = 3.0  KHz (for 25k   channel space) *W
  //         3 = 3.5  KHz
  //         4 = 4.5  KHz
  //         5 = 4.25 KHz
  //         6 = 4.0  KHz
  //         7 = 3.75 KHz

  static const uint8_t bs[] = {2, 2, 2, 2, 2, 2, 0, 0, 1, 1};

  // <5:4>   0 BW Mode Selection
  //         0 = 12.5k
  //         1 =  6.25k
  //         2 = 25k/20k
  //
  // <3>     1 ???
  //
  // <2>     0 Gain after FM Demodulation
  //         0 = 0dB
  //         1 = 6dB
  //
  // <1:0>   0 ???

  const uint16_t val =
      (0u << 15) |     //  0
      (rf[bw] << 12) | // *3 RF filter bandwidth
      // (wb[bw] << 9) |  // *0 RF filter bandwidth when signal is weak
      (rf[bw] << 9) | // *0 RF filter bandwidth when signal is weak
      (af[bw] << 6) | // *0 AFTxLPF2 filter Band Width
      (bs[bw] << 4) | //  2 BW Mode Selection 25K
      (1u << 3) |     //  1
      (0u << 2) |     //  0 Gain after FM Demodulation
      (0u << 0);      //  0
  BK4819_WriteRegister(BK4819_REG_43, val);
}

void BK4819_SetupPowerAmplifier(uint8_t Bias, uint32_t Frequency) {
  uint8_t Gain = Frequency < VHF_UHF_BOUND2 ? 0x08 : 0x22;

  // Enable PACTLoutput
  BK4819_WriteRegister(BK4819_REG_36, (Bias << 8) | 0x80U | Gain);
}

void BK4819_SetFrequency(uint32_t f) {
  // Log("BK f=%u", f);
  BK4819_WriteRegister(BK4819_REG_38, f & 0xFFFF);
  BK4819_WriteRegister(BK4819_REG_39, (f >> 16) & 0xFFFF);
}

uint32_t BK4819_GetFrequency(void) {
  return (BK4819_ReadRegister(BK4819_REG_39) << 16) |
         BK4819_ReadRegister(BK4819_REG_38);
}

void BK4819_SetupSquelch(SQL sq, uint8_t delayO, uint8_t delayC) {
  sq.no = Clamp(sq.no, 0, 127);
  sq.nc = Clamp(sq.nc, 0, 127);

  BK4819_WriteRegister(BK4819_REG_4D, 0xA000 | sq.gc);
  BK4819_WriteRegister(
      BK4819_REG_4E,
      (1u << 14) |                   //  1 ???
          (uint16_t)(delayO << 11) | // *5  squelch = open  delay .. 0 ~ 7
          (uint16_t)(delayC << 9) |  // *3  squelch = close delay .. 0 ~ 3
          sq.go);
  BK4819_WriteRegister(BK4819_REG_4F, (sq.nc << 8) | sq.no);
  BK4819_WriteRegister(BK4819_REG_78, (sq.ro << 8) | sq.rc);
}

void BK4819_Squelch(uint8_t sql, uint8_t OpenDelay, uint8_t CloseDelay) {
  BK4819_SetupSquelch(GetSql(sql), OpenDelay, CloseDelay);
}

void BK4819_SquelchType(SquelchType t) {
  BK4819_SetRegValue(RS_SQ_TYPE, squelchTypeValues[t]);
}

ModulationType BK4819_GetModulation() {
  uint16_t v = BK4819_ReadRegister(BK4819_REG_47) >> 8;
  for (uint8_t i = 0; i < ARRAY_SIZE(modTypeReg47Values); ++i) {
    if (modTypeReg47Values[i] == (v & 0b1111)) {
      return i;
    }
  }
  return MOD_FM;
}

void BK4819_SetAF(BK4819_AF_Type_t AF) {
  BK4819_WriteRegister(BK4819_REG_47, 0x6040 | (AF << 8));
}

uint16_t BK4819_GetRegValue(RegisterSpec s) {
  return (BK4819_ReadRegister(s.num) >> s.offset) & s.mask;
}

void BK4819_SetRegValue(RegisterSpec s, uint16_t v) {
  uint16_t reg = BK4819_ReadRegister(s.num);
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
}

void BK4819_SetAFC(uint8_t level) {
  if (level > 8) {
    level = 8;
  }

  if (level) {
    BK4819_WriteRegister(0x73, (8 - level) << 11);
  } else {
    BK4819_WriteRegister(0x73, (7 << 11) | (1 << 4));
  }
}

uint8_t BK4819_GetAFC() {
  uint16_t afc = BK4819_ReadRegister(0x73);
  if (((afc >> 4) & 1)) {
    return 0;
  }
  return 8 - ((afc >> 11) & 0b111);
}

XtalMode BK4819_XtalGet() {
  return (XtalMode)((BK4819_ReadRegister(0x3C) >> 6) & 0b11);
}

void BK4819_XtalSet(XtalMode mode) {
  uint16_t ifset = 0x2AAB;
  uint16_t xtal = 20360;
  switch (mode) {
  case XTAL_0_13M:
    xtal = 20232;
    ifset = 0x3555;
    // ctcCoef = 103190;
    break;
  case XTAL_1_19_2M:
    xtal = 20296;
    ifset = 0x2E39;
    // ctcCoef = 154740;
    break;
  case XTAL_2_26M:
    // ctcCoef = 206322;
    break;
  case XTAL_3_38_4M:
    xtal = 20424;
    ifset = 0x271C;
    // ctcCoef = 309500;
    break;
  }
  BK4819_WriteRegister(0x3C, xtal);
  BK4819_WriteRegister(0x3D, ifset);
}

static uint8_t oldMod = 255;

void BK4819_SetModulation(ModulationType type) {
  if (oldMod == type) {
    return;
  }

  if (type == MOD_BYP) {
    BK4819_EnterBypass();
  } else if (oldMod == MOD_BYP) {
    BK4819_ExitBypass();
  }

  oldMod = type;
  bool isSsb = type == MOD_LSB || type == MOD_USB;
  bool isFm = type == MOD_FM || type == MOD_WFM;
  BK4819_SetAF(modTypeReg47Values[type]);
  BK4819_SetRegValue(RS_AF_DAC_GAIN, 8);
  BK4819_SetRegValue(RS_AFC_DIS, !isFm);
  // return;
  if (type == MOD_WFM) {
    BK4819_SetRegValue(RS_RF_FILT_BW, 7);
    BK4819_SetRegValue(RS_RF_FILT_BW_WEAK, 7);
    BK4819_SetRegValue(RS_BW_MODE, 3);
    BK4819_XtalSet(XTAL_0_13M);
  } else if (isSsb) {
    BK4819_XtalSet(XTAL_3_38_4M);
    // BK4819_SetRegValue(RS_XTAL_MODE, 3);
    BK4819_SetRegValue(RS_IF_F, 0);
  } else {
    /* BK4819_SetRegValue(RS_XTAL_MODE, 2);
    BK4819_SetRegValue(RS_IF_F, 0x2AAB); */
    BK4819_XtalSet(XTAL_2_26M);
  }
}

void BK4819_RX_TurnOn(void) {
  // init bw, mod before that
  BK4819_WriteRegister(BK4819_REG_36, 0x0000);
  BK4819_WriteRegister(BK4819_REG_37, 0x1F0F);
  BK4819_WriteRegister(BK4819_REG_30, 0x0200);
  // SYS_DelayMs(10);
  BK4819_WriteRegister(
      BK4819_REG_30,
      BK4819_REG_30_ENABLE_VCO_CALIB | BK4819_REG_30_DISABLE_UNKNOWN |
          BK4819_REG_30_ENABLE_RX_LINK | BK4819_REG_30_ENABLE_AF_DAC |
          BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_PLL_VCO |
          BK4819_REG_30_DISABLE_PA_GAIN | BK4819_REG_30_DISABLE_MIC_ADC |
          BK4819_REG_30_DISABLE_TX_DSP | BK4819_REG_30_ENABLE_RX_DSP);
}

void BK4819_SelectFilterEx(Filter filter) {
  // Log("BK ---- SEL flt %u", filter);

  // for single write to 0x33
  const uint16_t PIN_BIT_VHF = 0x40U >> BK4819_GPIO4_PIN32_VHF_LNA;
  const uint16_t PIN_BIT_UHF = 0x40U >> BK4819_GPIO3_PIN31_UHF_LNA;

  if (filter == FILTER_VHF) {
    gBK4819_GpioOutState |= PIN_BIT_VHF;
  } else {
    gBK4819_GpioOutState &= ~PIN_BIT_VHF;
  }

  if (filter == FILTER_UHF) {
    gBK4819_GpioOutState |= PIN_BIT_UHF;
  } else {
    gBK4819_GpioOutState &= ~PIN_BIT_UHF;
  }

  BK4819_WriteRegister(BK4819_REG_33, gBK4819_GpioOutState);
}

void BK4819_SelectFilter(uint32_t f) {
  // Log("BK -- SEL flt for %u", f);
  Filter filter = f < SETTINGS_GetFilterBound() ? FILTER_VHF : FILTER_UHF;

  if (selectedFilter != filter) {
    selectedFilter = filter;
    BK4819_SelectFilterEx(filter);
  }
}

void BK4819_DisableScramble(void) {
  uint16_t v = BK4819_ReadRegister(BK4819_REG_31);
  BK4819_WriteRegister(BK4819_REG_31, v & 0xFFFD);
}

void BK4819_EnableScramble(uint8_t type) {
  uint16_t v = BK4819_ReadRegister(BK4819_REG_31);
  BK4819_WriteRegister(BK4819_REG_31, v | 2);
  BK4819_WriteRegister(BK4819_REG_71, (type * 0x0408) + 0x68DC);
}

void BK4819_SetScrambler(uint8_t type) {
  if (type) {
    BK4819_EnableScramble(type);
  } else {
    BK4819_DisableScramble();
  }
}

void BK4819_DisableVox(void) {
  uint16_t v = BK4819_ReadRegister(BK4819_REG_31);
  BK4819_WriteRegister(BK4819_REG_31, v & 0xFFFB);
}

void BK4819_DisableDTMF(void) { BK4819_WriteRegister(BK4819_REG_24, 0); }

void BK4819_EnableDTMF(void) {
  BK4819_WriteRegister(BK4819_REG_21, 0x06D8);
  BK4819_WriteRegister(BK4819_REG_24,
                       0 | (1U << BK4819_REG_24_SHIFT_UNKNOWN_15) |
                           (24 << BK4819_REG_24_SHIFT_THRESHOLD) |
                           (1U << BK4819_REG_24_SHIFT_UNKNOWN_6) |
                           BK4819_REG_24_ENABLE | BK4819_REG_24_SELECT_DTMF |
                           (14U << BK4819_REG_24_SHIFT_MAX_SYMBOLS));
}

void BK4819_PlayTone(uint16_t Frequency, bool bTuningGainSwitch) {
  BK4819_EnterTxMute();
  BK4819_SetAF(BK4819_AF_BEEP);

  uint8_t gain = bTuningGainSwitch ? 28 : 96;

  uint16_t toneCfg = BK4819_REG_70_ENABLE_TONE1 |
                     (gain << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN);
  BK4819_WriteRegister(BK4819_REG_70, toneCfg);

  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_30, 0 | BK4819_REG_30_ENABLE_AF_DAC |
                                          BK4819_REG_30_ENABLE_DISC_MODE |
                                          BK4819_REG_30_ENABLE_TX_DSP);

  BK4819_SetToneFrequency(Frequency);
}

void BK4819_EnterTxMute(void) { BK4819_WriteRegister(BK4819_REG_50, 0xBB20); }

void BK4819_ExitTxMute(void) { BK4819_WriteRegister(BK4819_REG_50, 0x3B20); }

void BK4819_Sleep(void) {
  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

void BK4819_TurnsOffTones_TurnsOnRX(void) {
  BK4819_WriteRegister(BK4819_REG_70, 0);
  BK4819_SetAF(BK4819_AF_MUTE);
  BK4819_ExitTxMute();
  BK4819_Idle();
  BK4819_WriteRegister(
      BK4819_REG_30,
      0 | BK4819_REG_30_ENABLE_VCO_CALIB | BK4819_REG_30_ENABLE_RX_LINK |
          BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE |
          BK4819_REG_30_ENABLE_PLL_VCO | BK4819_REG_30_ENABLE_RX_DSP);
}

void BK4819_ResetFSK(void) {
  BK4819_WriteRegister(BK4819_REG_3F, 0x0000); // Disable interrupts
  BK4819_WriteRegister(BK4819_REG_59,
                       0x0068); // Sync length 4 bytes, 7 byte preamble
  SYS_DelayMs(30);
  BK4819_Idle();
}

void BK4819_FskClearFifo(void) {
  const uint16_t fsk_reg59 = BK4819_ReadRegister(BK4819_REG_59);
  BK4819_WriteRegister(BK4819_REG_59, (1u << 15) | (1u << 14) | fsk_reg59);
}

void BK4819_FskEnableRx(void) {
  const uint16_t fsk_reg59 = BK4819_ReadRegister(BK4819_REG_59);
  BK4819_WriteRegister(BK4819_REG_59, (1u << 12) | fsk_reg59);
}

void BK4819_FskEnableTx(void) {
  const uint16_t fsk_reg59 = BK4819_ReadRegister(BK4819_REG_59);
  BK4819_WriteRegister(BK4819_REG_59, (1u << 11) | fsk_reg59);
}

void BK4819_EnterBypass() {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_7E);
  BK4819_WriteRegister(BK4819_REG_7E, reg & ~(0b111 << 3) & ~(0b111 << 0));
}

void BK4819_ExitBypass(void) {
  BK4819_SetAF(BK4819_AF_MUTE);
  /* uint16_t reg = BK4819_ReadRegister(BK4819_REG_7E);
  BK4819_WriteRegister(BK4819_REG_7E, reg & ~(0b111 << 3) & ~(0b111 << 0) |
                                          (0b101 << 3) | (0b110 << 0)); */
  BK4819_WriteRegister(BK4819_REG_7E, 0x302E);
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

void BK4819_ExitSubAu(void) { BK4819_WriteRegister(BK4819_REG_51, 0x0000); }

void BK4819_EnterDTMF_TX(bool bLocalLoopback) {
  BK4819_EnableDTMF();
  BK4819_EnterTxMute();
  if (bLocalLoopback) {
    BK4819_SetAF(BK4819_AF_BEEP);
  } else {
    BK4819_SetAF(BK4819_AF_MUTE);
  }
  BK4819_WriteRegister(BK4819_REG_70,
                       0 | BK4819_REG_70_MASK_ENABLE_TONE1 |
                           (83 << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN) |
                           BK4819_REG_70_MASK_ENABLE_TONE2 |
                           (83 << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN));

  BK4819_EnableTXLink();
}

void BK4819_ExitDTMF_TX(bool bKeep) {
  BK4819_EnterTxMute();
  BK4819_SetAF(BK4819_AF_MUTE);
  BK4819_WriteRegister(BK4819_REG_70, 0x0000);
  BK4819_DisableDTMF();
  BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
  if (!bKeep) {
    BK4819_ExitTxMute();
  }
}

void BK4819_EnableTXLink(void) {
  BK4819_WriteRegister(
      BK4819_REG_30,
      0 | BK4819_REG_30_ENABLE_VCO_CALIB | BK4819_REG_30_ENABLE_UNKNOWN |
          BK4819_REG_30_DISABLE_RX_LINK | BK4819_REG_30_ENABLE_AF_DAC |
          BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_PLL_VCO |
          BK4819_REG_30_ENABLE_PA_GAIN | BK4819_REG_30_DISABLE_MIC_ADC |
          BK4819_REG_30_ENABLE_TX_DSP | BK4819_REG_30_DISABLE_RX_DSP);
}

void BK4819_PlayDTMF(char Code) {
  switch (Code) {
  case '0':
    BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
    BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
    break;
  case '1':
    BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
    BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
    break;
  case '2':
    BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
    BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
    break;
  case '3':
    BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
    BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
    break;
  case '4':
    BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
    BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
    break;
  case '5':
    BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
    BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
    break;
  case '6':
    BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
    BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
    break;
  case '7':
    BK4819_WriteRegister(BK4819_REG_71, 0x225C);
    BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
    break;
  case '8':
    BK4819_WriteRegister(BK4819_REG_71, 0x225c);
    BK4819_WriteRegister(BK4819_REG_72, 0x35E1);
    break;
  case '9':
    BK4819_WriteRegister(BK4819_REG_71, 0x225C);
    BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
    break;
  case 'A':
    BK4819_WriteRegister(BK4819_REG_71, 0x1C1C);
    BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
    break;
  case 'B':
    BK4819_WriteRegister(BK4819_REG_71, 0x1F0E);
    BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
    break;
  case 'C':
    BK4819_WriteRegister(BK4819_REG_71, 0x225C);
    BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
    break;
  case 'D':
    BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
    BK4819_WriteRegister(BK4819_REG_72, 0x41DC);
    break;
  case '*':
    BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
    BK4819_WriteRegister(BK4819_REG_72, 0x30C2);
    break;
  case '#':
    BK4819_WriteRegister(BK4819_REG_71, 0x25F3);
    BK4819_WriteRegister(BK4819_REG_72, 0x3B91);
    break;
  }
}

void BK4819_PlayDTMFString(const char *pString, bool bDelayFirst,
                           uint16_t FirstCodePersistTime,
                           uint16_t HashCodePersistTime,
                           uint16_t CodePersistTime,
                           uint16_t CodeInternalTime) {
  uint16_t delay;

  for (uint8_t i = 0; pString[i]; i++) {
    BK4819_PlayDTMF(pString[i]);
    BK4819_ExitTxMute();
    if (bDelayFirst && i == 0) {
      delay = FirstCodePersistTime;
    } else if (pString[i] == '*' || pString[i] == '#') {
      delay = HashCodePersistTime;
    } else {
      delay = CodePersistTime;
    }
    SYS_DelayMs(delay);
    BK4819_EnterTxMute();
    SYS_DelayMs(CodeInternalTime);
  }
}

void BK4819_TransmitTone(uint32_t Frequency) {
  BK4819_EnterTxMute();
  BK4819_WriteRegister(BK4819_REG_70,
                       BK4819_REG_70_MASK_ENABLE_TONE1 |
                           (56 << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
  BK4819_SetToneFrequency(Frequency);

  BK4819_SetAF(gSettings.toneLocal ? BK4819_AF_BEEP : BK4819_AF_MUTE);

  BK4819_EnableTXLink();
  BK4819_ExitTxMute();
}

void BK4819_GenTail(uint8_t Tail) {
  switch (Tail) {
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
  case 4: // 55Hz tone freq NOTE: REG_07
    BK4819_WriteRegister(BK4819_REG_07, 0x046f);
    break;
  }
}

void BK4819_EnableCDCSS(void) {
  BK4819_GenTail(0); // CTC134
  BK4819_WriteRegister(BK4819_REG_51, 0x804A);
}

void BK4819_EnableCTCSS(void) {
  // BK4819_GenTail(2); // CTC180
  BK4819_GenTail(4); // CTC55
  BK4819_WriteRegister(BK4819_REG_51, 0x904A);
}

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

// AF TX/RX input amplitude (dB)
uint8_t BK4819_GetAfTxRx(void) {
  return BK4819_ReadRegister(BK4819_REG_6F) & 0x3F;
}

uint8_t BK4819_GetSNR(void) { return BK4819_ReadRegister(0x61) & 0xFF; }

uint16_t BK4819_GetVoiceAmplitude(void) { return BK4819_ReadRegister(0x64); }

bool BK4819_GetFrequencyScanResult(uint32_t *pFrequency) {
  uint16_t High = BK4819_ReadRegister(BK4819_REG_0D);
  bool Finished = (High & 0x8000) == 0;

  if (Finished) {
    uint16_t Low = BK4819_ReadRegister(BK4819_REG_0E);
    *pFrequency = (uint32_t)((High & 0x7FF) << 16) | Low;
  }

  return Finished;
}

BK4819_CssScanResult_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq,
                                                 uint16_t *pCtcssFreq) {
  uint16_t Low;
  uint16_t High = BK4819_ReadRegister(BK4819_REG_69);

  if ((High & 0x8000) == 0) {
    Low = BK4819_ReadRegister(BK4819_REG_6A);
    *pCdcssFreq = ((High & 0xFFF) << 12) | (Low & 0xFFF);
    return BK4819_CSS_RESULT_CDCSS;
  }

  Low = BK4819_ReadRegister(BK4819_REG_68);

  if ((Low & 0x8000) == 0) {
    *pCtcssFreq = ((Low & 0x1FFF) * 4843) / 10000;
    return BK4819_CSS_RESULT_CTCSS;
  }

  return BK4819_CSS_RESULT_NOT_FOUND;
}

void BK4819_DisableFrequencyScan(void) {
  BK4819_WriteRegister(BK4819_REG_32, 0x0244);
}

void BK4819_EnableFrequencyScan(void) {
  BK4819_WriteRegister(BK4819_REG_32, 0x0245);
  // BK4819_WriteRegister(BK4819_REG_32, 0x0B01);
  // BK4819_WriteRegister(BK4819_REG_32, (F_SC_T_0_4s << 14) | (2047 << 1) |
  // true);
}

void BK4819_EnableFrequencyScanEx(FreqScanTime t) {
  // BK4819_WriteRegister(BK4819_REG_32, 0x0B01 | (t << 14));
  BK4819_WriteRegister(BK4819_REG_32, 0x0245 | (t << 14));
  // BK4819_WriteRegister(BK4819_REG_32, 0x3fff | (t << 14));
  // BK4819_WriteRegister(BK4819_REG_32, (t << 14) | (2047 << 1) | true);
}

void BK4819_EnableFrequencyScanEx2(FreqScanTime t, uint16_t hz) {
  // BK4819_WriteRegister(BK4819_REG_32, 0x0B01 | (t << 14));
  // BK4819_WriteRegister(BK4819_REG_32, 0x0245 | (t << 14));
  // BK4819_WriteRegister(BK4819_REG_32, 0x3fff | (t << 14));
  BK4819_WriteRegister(BK4819_REG_32, (t << 14) | (hz << 1) | true);
}

void BK4819_StopScan(void) {
  BK4819_DisableFrequencyScan();
  BK4819_Idle();
}

uint8_t BK4819_GetDTMF_5TONE_Code(void) {
  return (BK4819_ReadRegister(BK4819_REG_0B) >> 8) & 0x0F;
}

uint8_t BK4819_GetCDCSSCodeType(void) {
  return (BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3;
}

uint8_t BK4819_GetCTCType(void) {
  return (BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3;
}

void BK4819_PlayRogerTiny(void) {
  const uint16_t M[] = {1250, 20, 0, 10, 1500, 20, 0, 0};
  BK4819_PlaySequence(M);
}

void BK4819_PlaySequence(const uint16_t *M) {
  bool initialTone = true;
  for (uint8_t i = 0; i < 255; i += 2) {
    uint16_t note = M[i];
    uint16_t t = M[i + 1];
    if (!note && !t) {
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
    if (note && !t) {
      return;
    }
    SYS_DelayMs(t);
  }
  if (gSettings.toneLocal) {
    AUDIO_ToggleSpeaker(false);
    SYS_DelayMs(10);
  }
  BK4819_EnterTxMute();
}

void BK4819_Enable_AfDac_DiscMode_TxDsp(void) {
  BK4819_Idle();
  BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

void BK4819_GetVoxAmp(uint16_t *pResult) {
  *pResult = BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF;
}

// todo: make single fn
void BK4819_PlayDTMFEx(bool bLocalLoopback, char Code) {
  BK4819_EnableDTMF();
  BK4819_EnterTxMute();
  BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
  BK4819_WriteRegister(BK4819_REG_70, 0xD3D3);
  BK4819_EnableTXLink();
  SYS_DelayMs(50);
  BK4819_PlayDTMF(Code);
  BK4819_ExitTxMute();
}

void BK4819_ToggleAFBit(bool on) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  reg &= ~(1 << 8);
  if (on)
    reg |= 1 << 8;
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

void BK4819_ToggleAFDAC(bool on) {
  uint16_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~BK4819_REG_30_ENABLE_AF_DAC;
  if (on)
    Reg |= BK4819_REG_30_ENABLE_AF_DAC;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

void BK4819_TuneTo(uint32_t f, bool precise) {
  static uint32_t oldFreq;
  if (oldFreq == f) { // TODO: maybe save current freq locally
    return;
  }
  BK4819_SelectFilter(f);
  BK4819_SetFrequency(f);
  oldFreq = f;
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  if (precise) {
    BK4819_WriteRegister(BK4819_REG_30, 0x0200);
  } else {
    BK4819_WriteRegister(BK4819_REG_30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
  }
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

void BK4819_SetToneFrequency(uint16_t f) {
  BK4819_WriteRegister(BK4819_REG_71, scale_freq(f));
}

void BK4819_SetTone2Frequency(uint16_t f) {
  BK4819_WriteRegister(BK4819_REG_72, scale_freq(f));
}

bool BK4819_IsSquelchOpen(void) {
  return (BK4819_ReadRegister(BK4819_REG_0C) >> 1) & 1;
}

void BK4819_ResetRSSI(void) {
  uint16_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
  Reg |= 1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}
