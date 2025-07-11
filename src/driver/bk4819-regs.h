/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef BK4819_REGS_H
#define BK4819_REGS_H

#include <stdint.h>
typedef struct RegisterSpec {
  const char *name;
  uint8_t num;
  uint8_t offset;
  uint16_t mask;
  uint16_t inc;
} RegisterSpec;

static const RegisterSpec RS_AFC_DIS = {"AFC Dis", 0x73, 4, 1, 1};
static const RegisterSpec RS_AF_OUT = {"AF Out", 0x47, 8, 0xF, 1};
static const RegisterSpec RS_AF_DAC_GAIN = {"AF DAC G", 0x48, 0, 0xF, 1};
static const RegisterSpec RS_AF_RX_GAIN = {"AF RX G", 0x48, 4, 0b111111, 1};
static const RegisterSpec RS_XTAL_MODE = {"XtalFMode", 0x3C, 6, 0b11, 1};
static const RegisterSpec RS_RF_FILT_BW = {"RF BW", 0x43, 12, 0b111, 1};
static const RegisterSpec RS_RF_FILT_BW_WEAK = {"RF BWw", 0x43, 9, 0b111, 1};
static const RegisterSpec RS_BW_MODE = {"BW Mode", 0x43, 4, 0b11, 1};
static const RegisterSpec RS_IF_F = {"IF", 0x3D, 0, 0xFFFF, 1};
static const RegisterSpec RS_SQ_TYPE = {"SQ type", 0x77, 8, 0xFF, 1};
static const RegisterSpec RS_DEV = {"DEV", 0x40, 0, 0xFFF, 10};
static const RegisterSpec RS_PEAK_RSSI = {"Peak RSSI", 0x62, 0, 0xFF, 1};
static const RegisterSpec RS_MIC = {"MIC", 0x7D, 0, 0xF, 1};
static const RegisterSpec RS_AVC = {"AVC", 0x4B, 5, 1, 1};
/* {"Gain", BK4819_REG_13, 0, 0xFFFF, 1},

{"IF", 0x3D, 0, 0xFFFF, 100},

{"DEV", 0x40, 0, 0xFFF, 10},
{"CMP", 0x31, 3, 1, 1},

{"AGCL", 0x49, 0, 0b1111111, 1},
{"AGCH", 0x49, 7, 0b1111111, 1},
{"AFC", 0x73, 0, 0xFF, 1}, */

enum BK4819_REGISTER_t {
  BK4819_REG_00 = 0x00U,
  BK4819_REG_02 = 0x02U,
  BK4819_REG_06 = 0x06U,
  BK4819_REG_07 = 0x07U,
  BK4819_REG_08 = 0x08U,
  BK4819_REG_09 = 0x09U,
  BK4819_REG_0B = 0x0BU,
  BK4819_REG_0C = 0x0CU,
  BK4819_REG_0D = 0x0DU,
  BK4819_REG_0E = 0x0EU,
  BK4819_REG_10 = 0x10U,
  BK4819_REG_11 = 0x11U,
  BK4819_REG_12 = 0x12U,
  BK4819_REG_13 = 0x13U,
  BK4819_REG_14 = 0x14U,
  BK4819_REG_19 = 0x19U,
  BK4819_REG_1E = 0x1EU,
  BK4819_REG_1F = 0x1FU,
  BK4819_REG_20 = 0x20U,
  BK4819_REG_21 = 0x21U,
  BK4819_REG_24 = 0x24U,
  BK4819_REG_28 = 0x28U,
  BK4819_REG_29 = 0x29U,
  BK4819_REG_2B = 0x2BU,
  BK4819_REG_30 = 0x30U,
  BK4819_REG_31 = 0x31U,
  BK4819_REG_32 = 0x32U,
  BK4819_REG_33 = 0x33U,
  BK4819_REG_36 = 0x36U,
  BK4819_REG_37 = 0x37U,
  BK4819_REG_38 = 0x38U,
  BK4819_REG_39 = 0x39U,
  BK4819_REG_3A = 0x3AU,
  BK4819_REG_3B = 0x3BU,
  BK4819_REG_3C = 0x3CU,
  BK4819_REG_3E = 0x3EU,
  BK4819_REG_3F = 0x3FU,
  BK4819_REG_40 = 0x40U,
  BK4819_REG_43 = 0x43U,
  BK4819_REG_46 = 0x46U,
  BK4819_REG_47 = 0x47U,
  BK4819_REG_48 = 0x48U,
  BK4819_REG_49 = 0x49U,
  BK4819_REG_4D = 0x4DU,
  BK4819_REG_4E = 0x4EU,
  BK4819_REG_4F = 0x4FU,
  BK4819_REG_50 = 0x50U,
  BK4819_REG_51 = 0x51U,
  BK4819_REG_52 = 0x52U,
  BK4819_REG_58 = 0x58U,
  BK4819_REG_59 = 0x59U,
  BK4819_REG_5A = 0x5AU,
  BK4819_REG_5B = 0x5BU,
  BK4819_REG_5C = 0x5CU,
  BK4819_REG_5D = 0x5DU,
  BK4819_REG_5E = 0x5EU,
  BK4819_REG_5F = 0x5FU,
  BK4819_REG_63 = 0x63U,
  BK4819_REG_64 = 0x64U,
  BK4819_REG_65 = 0x65U,
  BK4819_REG_67 = 0x67U,
  BK4819_REG_68 = 0x68U,
  BK4819_REG_69 = 0x69U,
  BK4819_REG_6A = 0x6AU,
  BK4819_REG_6F = 0x6FU,
  BK4819_REG_70 = 0x70U,
  BK4819_REG_71 = 0x71U,
  BK4819_REG_72 = 0x72U,
  BK4819_REG_78 = 0x78U,
  BK4819_REG_79 = 0x79U,
  BK4819_REG_7A = 0x7AU,
  BK4819_REG_7B = 0x7BU,
  BK4819_REG_7C = 0x7CU,
  BK4819_REG_7D = 0x7DU,
  BK4819_REG_7E = 0x7EU,
};

typedef enum BK4819_REGISTER_t BK4819_REGISTER_t;

enum BK4819_GPIO_PIN_t {
  BK4819_GPIO0_PIN28_RX_ENABLE = 0,
  BK4819_GPIO1_PIN29_PA_ENABLE = 1,
  BK4819_GPIO4_PIN32 = 2,
  BK4819_GPIO3_PIN31_UHF_LNA = 3,
  BK4819_GPIO4_PIN32_VHF_LNA = 4,
  BK4819_GPIO5_PIN1_RED = 5,
  BK4819_GPIO0_PIN28_GREEN = 6,
};

typedef enum BK4819_GPIO_PIN_t BK4819_GPIO_PIN_t;

// REG 02

#define BK4819_REG_02_SHIFT_FSK_TX_FINISHED 15
#define BK4819_REG_02_SHIFT_FSK_FIFO_ALMOST_EMPTY 14
#define BK4819_REG_02_SHIFT_FSK_RX_FINISHED 13
#define BK4819_REG_02_SHIFT_FSK_FIFO_ALMOST_FULL 12
#define BK4819_REG_02_SHIFT_DTMF_5TONE_FOUND 11
#define BK4819_REG_02_SHIFT_CxCSS_TAIL 10
#define BK4819_REG_02_SHIFT_CDCSS_FOUND 9
#define BK4819_REG_02_SHIFT_CDCSS_LOST 8
#define BK4819_REG_02_SHIFT_CTCSS_FOUND 7
#define BK4819_REG_02_SHIFT_CTCSS_LOST 6
#define BK4819_REG_02_SHIFT_VOX_FOUND 5
#define BK4819_REG_02_SHIFT_VOX_LOST 4
#define BK4819_REG_02_SHIFT_SQUELCH_FOUND 3
#define BK4819_REG_02_SHIFT_SQUELCH_LOST 2
#define BK4819_REG_02_SHIFT_FSK_RX_SYNC 1

#define BK4819_REG_02_MASK_FSK_TX_FINISHED (1U << BK4819_REG_02_SHIFT_FSK_TX)
#define BK4819_REG_02_MASK_FSK_FIFO_ALMOST_EMPTY                               \
  (1U << BK4819_REG_02_SHIFT_FSK_FIFO_ALMOST_EMPTY)
#define BK4819_REG_02_MASK_FSK_RX_FINISHED                                     \
  (1U << BK4819_REG_02_SHIFT_FSK_RX_FINISHED)
#define BK4819_REG_02_MASK_FSK_FIFO_ALMOST_FULL                                \
  (1U << BK4819_REG_02_SHIFT_FSK_FIFO_ALMOST_FULL)
#define BK4819_REG_02_MASK_DTMF_5TONE_FOUND                                    \
  (1U << BK4819_REG_02_SHIFT_DTMF_5TONE_FOUND)
#define BK4819_REG_02_MASK_CxCSS_TAIL (1U << BK4819_REG_02_SHIFT_CxCSS_TAIL)
#define BK4819_REG_02_MASK_CDCSS_FOUND (1U << BK4819_REG_02_SHIFT_CDCSS_FOUND)
#define BK4819_REG_02_MASK_CDCSS_LOST (1U << BK4819_REG_02_SHIFT_CDCSS_LOST)
#define BK4819_REG_02_MASK_CTCSS_FOUND (1U << BK4819_REG_02_SHIFT_CTCSS_FOUND)
#define BK4819_REG_02_MASK_CTCSS_LOST (1U << BK4819_REG_02_SHIFT_CTCSS_LOST)
#define BK4819_REG_02_MASK_VOX_FOUND (1U << BK4819_REG_02_SHIFT_VOX_FOUND)
#define BK4819_REG_02_MASK_VOX_LOST (1U << BK4819_REG_02_SHIFT_VOX_LOST)
#define BK4819_REG_02_MASK_SQUELCH_FOUND                                       \
  (1U << BK4819_REG_02_SHIFT_SQUELCH_FOUND)
#define BK4819_REG_02_MASK_SQUELCH_LOST (1U << BK4819_REG_02_SHIFT_SQUELCH_LOST)
#define BK4819_REG_02_MASK_FSK_RX_SYNC (1U << BK4819_REG_02_SHIFT_FSK_RX_SYNC)

#define BK4819_REG_02_FSK_TX_FINISHED                                          \
  (1U << BK4819_REG_02_SHIFT_FSK_TX_FINISHED)
#define BK4819_REG_02_FSK_FIFO_ALMOST_EMPTY                                    \
  (1U << BK4819_REG_02_SHIFT_FSK_FIFO_ALMOST_EMPTY)
#define BK4819_REG_02_FSK_RX_FINISHED                                          \
  (1U << BK4819_REG_02_SHIFT_FSK_RX_FINISHED)
#define BK4819_REG_02_FSK_FIFO_ALMOST_FULL                                     \
  (1U << BK4819_REG_02_SHIFT_FSK_FIFO_ALMOST_FULL)
#define BK4819_REG_02_DTMF_5TONE_FOUND                                         \
  (1U << BK4819_REG_02_SHIFT_DTMF_5TONE_FOUND)
#define BK4819_REG_02_CxCSS_TAIL (1U << BK4819_REG_02_SHIFT_CxCSS_TAIL)
#define BK4819_REG_02_CDCSS_FOUND (1U << BK4819_REG_02_SHIFT_CDCSS_FOUND)
#define BK4819_REG_02_CDCSS_LOST (1U << BK4819_REG_02_SHIFT_CDCSS_LOST)
#define BK4819_REG_02_CTCSS_FOUND (1U << BK4819_REG_02_SHIFT_CTCSS_FOUND)
#define BK4819_REG_02_CTCSS_LOST (1U << BK4819_REG_02_SHIFT_CTCSS_LOST)
#define BK4819_REG_02_VOX_FOUND (1U << BK4819_REG_02_SHIFT_VOX_FOUND)
#define BK4819_REG_02_VOX_LOST (1U << BK4819_REG_02_SHIFT_VOX_LOST)
#define BK4819_REG_02_SQUELCH_FOUND (1U << BK4819_REG_02_SHIFT_SQUELCH_FOUND)
#define BK4819_REG_02_SQUELCH_LOST (1U << BK4819_REG_02_SHIFT_SQUELCH_LOST)
#define BK4819_REG_02_FSK_RX_SYNC (1U << BK4819_REG_02_SHIFT_FSK_RX_SYNC)

// REG 07

#define BK4819_REG_07_SHIFT_FREQUENCY_MODE 13
#define BK4819_REG_07_SHIFT_FREQUENCY 0

#define BK4819_REG_07_MASK_FREQUENCY_MODE                                      \
  (0x0007U << BK4819_REG_07_SHIFT_FREQUENCY_MODE)
#define BK4819_REG_07_MASK_FREQUENCY (0x1FFFU << BK4819_REG_07_SHIFT_FREQUENCY)

#define BK4819_REG_07_MODE_CTC1 (0U << BK4819_REG_07_SHIFT_FREQUENCY_MODE)
#define BK4819_REG_07_MODE_CTC2 (1U << BK4819_REG_07_SHIFT_FREQUENCY_MODE)
#define BK4819_REG_07_MODE_CDCSS (2U << BK4819_REG_07_SHIFT_FREQUENCY_MODE)

// REG 24

#define BK4819_REG_24_SHIFT_UNKNOWN_15 15
#define BK4819_REG_24_SHIFT_THRESHOLD 7
#define BK4819_REG_24_SHIFT_UNKNOWN_6 6
#define BK4819_REG_24_SHIFT_ENABLE 5
#define BK4819_REG_24_SHIFT_SELECT 4
#define BK4819_REG_24_SHIFT_MAX_SYMBOLS 0

#define BK4819_REG_24_MASK_THRESHOLD (0x2FU << BK4819_REG_24_SHIFT_THRESHOLD)
#define BK4819_REG_24_MASK_ENABLE (0x01U << BK4819_REG_24_SHIFT_ENABLE)
#define BK4819_REG_24_MASK_SELECT (0x04U << BK4819_REG_24_SHIFT_SELECT)
#define BK4819_REG_24_MASK_MAX_SYMBOLS                                         \
  (0x0FU << BK4819_REG_24_SHIFT_MAX_SYMBOLS)

#define BK4819_REG_24_ENABLE (0x01U << BK4819_REG_24_SHIFT_ENABLE)
#define BK4819_REG_24_DISABLE (0x00U << BK4819_REG_24_SHIFT_ENABLE)
#define BK4819_REG_24_SELECT_DTMF (0x01U << BK4819_REG_24_SHIFT_SELECT)
#define BK4819_REG_24_SELECT_SELCALL (0x00U << BK4819_REG_24_SHIFT_SELECT)

// REG 30

#define BK4819_REG_30_SHIFT_ENABLE_VCO_CALIB 15
#define BK4819_REG_30_SHIFT_ENABLE_UNKNOWN 14
#define BK4819_REG_30_SHIFT_ENABLE_RX_LINK 10
#define BK4819_REG_30_SHIFT_ENABLE_AF_DAC 9
#define BK4819_REG_30_SHIFT_ENABLE_DISC_MODE 8
#define BK4819_REG_30_SHIFT_ENABLE_PLL_VCO 4
#define BK4819_REG_30_SHIFT_ENABLE_PA_GAIN 3
#define BK4819_REG_30_SHIFT_ENABLE_MIC_ADC 2
#define BK4819_REG_30_SHIFT_ENABLE_TX_DSP 1
#define BK4819_REG_30_SHIFT_ENABLE_RX_DSP 0

#define BK4819_REG_30_MASK_ENABLE_VCO_CALIB                                    \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_VCO_CALIB)
#define BK4819_REG_30_MASK_ENABLE_UNKNOWN                                      \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_UNKNOWN)
#define BK4819_REG_30_MASK_ENABLE_RX_LINK                                      \
  (0xFU << BK4819_REG_30_SHIFT_ENABLE_RX_LINK)
#define BK4819_REG_30_MASK_ENABLE_AF_DAC                                       \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_AF_DAC)
#define BK4819_REG_30_MASK_ENABLE_DISC_MODE                                    \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_DISC_MODE)
#define BK4819_REG_30_MASK_ENABLE_PLL_VCO                                      \
  (0xFU << BK4819_REG_30_SHIFT_ENABLE_PLL_VCO)
#define BK4819_REG_30_MASK_ENABLE_PA_GAIN                                      \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_PA_GAIN)
#define BK4819_REG_30_MASK_ENABLE_MIC_ADC                                      \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_MIC_ADC)
#define BK4819_REG_30_MASK_ENABLE_TX_DSP                                       \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_TX_DSP)
#define BK4819_REG_30_MASK_ENABLE_RX_DSP                                       \
  (0x1U << BK4819_REG_30_SHIFT_ENABLE_RX_DSP)

enum {
  BK4819_REG_30_ENABLE_VCO_CALIB =
      (0x1U << BK4819_REG_30_SHIFT_ENABLE_VCO_CALIB),
  BK4819_REG_30_DISABLE_VCO_CALIB =
      (0x0U << BK4819_REG_30_SHIFT_ENABLE_VCO_CALIB),
  BK4819_REG_30_ENABLE_UNKNOWN = (0x1U << BK4819_REG_30_SHIFT_ENABLE_UNKNOWN),
  BK4819_REG_30_DISABLE_UNKNOWN = (0x0U << BK4819_REG_30_SHIFT_ENABLE_UNKNOWN),
  BK4819_REG_30_ENABLE_RX_LINK = (0xFU << BK4819_REG_30_SHIFT_ENABLE_RX_LINK),
  BK4819_REG_30_DISABLE_RX_LINK = (0x0U << BK4819_REG_30_SHIFT_ENABLE_RX_LINK),
  BK4819_REG_30_ENABLE_AF_DAC = (0x1U << BK4819_REG_30_SHIFT_ENABLE_AF_DAC),
  BK4819_REG_30_DISABLE_AF_DAC = (0x0U << BK4819_REG_30_SHIFT_ENABLE_AF_DAC),
  BK4819_REG_30_ENABLE_DISC_MODE =
      (0x1U << BK4819_REG_30_SHIFT_ENABLE_DISC_MODE),
  BK4819_REG_30_DISABLE_DISC_MODE =
      (0x0U << BK4819_REG_30_SHIFT_ENABLE_DISC_MODE),
  BK4819_REG_30_ENABLE_PLL_VCO = (0xFU << BK4819_REG_30_SHIFT_ENABLE_PLL_VCO),
  BK4819_REG_30_DISABLE_PLL_VCO = (0x0U << BK4819_REG_30_SHIFT_ENABLE_PLL_VCO),
  BK4819_REG_30_ENABLE_PA_GAIN = (0x1U << BK4819_REG_30_SHIFT_ENABLE_PA_GAIN),
  BK4819_REG_30_DISABLE_PA_GAIN = (0x0U << BK4819_REG_30_SHIFT_ENABLE_PA_GAIN),
  BK4819_REG_30_ENABLE_MIC_ADC = (0x1U << BK4819_REG_30_SHIFT_ENABLE_MIC_ADC),
  BK4819_REG_30_DISABLE_MIC_ADC = (0x0U << BK4819_REG_30_SHIFT_ENABLE_MIC_ADC),
  BK4819_REG_30_ENABLE_TX_DSP = (0x1U << BK4819_REG_30_SHIFT_ENABLE_TX_DSP),
  BK4819_REG_30_DISABLE_TX_DSP = (0x0U << BK4819_REG_30_SHIFT_ENABLE_TX_DSP),
  BK4819_REG_30_ENABLE_RX_DSP = (0x1U << BK4819_REG_30_SHIFT_ENABLE_RX_DSP),
  BK4819_REG_30_DISABLE_RX_DSP = (0x0U << BK4819_REG_30_SHIFT_ENABLE_RX_DSP),
};

// REG 3F

#define BK4819_REG_3F_SHIFT_FSK_TX_FINISHED 15
#define BK4819_REG_3F_SHIFT_FSK_FIFO_ALMOST_EMPTY 14
#define BK4819_REG_3F_SHIFT_FSK_RX_FINISHED 13
#define BK4819_REG_3F_SHIFT_FSK_FIFO_ALMOST_FULL 12
#define BK4819_REG_3F_SHIFT_DTMF_5TONE_FOUND 11
#define BK4819_REG_3F_SHIFT_CxCSS_TAIL 10
#define BK4819_REG_3F_SHIFT_CDCSS_FOUND 9
#define BK4819_REG_3F_SHIFT_CDCSS_LOST 8
#define BK4819_REG_3F_SHIFT_CTCSS_FOUND 7
#define BK4819_REG_3F_SHIFT_CTCSS_LOST 6
#define BK4819_REG_3F_SHIFT_VOX_FOUND 5
#define BK4819_REG_3F_SHIFT_VOX_LOST 4
#define BK4819_REG_3F_SHIFT_SQUELCH_FOUND 3
#define BK4819_REG_3F_SHIFT_SQUELCH_LOST 2
#define BK4819_REG_3F_SHIFT_FSK_RX_SYNC 1

#define BK4819_REG_3F_MASK_FSK_TX_FINISHED (1U << BK4819_REG_3F_SHIFT_FSK_TX)
#define BK4819_REG_3F_MASK_FSK_FIFO_ALMOST_EMPTY                               \
  (1U << BK4819_REG_3F_SHIFT_FSK_FIFO_ALMOST_EMPTY)
#define BK4819_REG_3F_MASK_FSK_RX_FINISHED                                     \
  (1U << BK4819_REG_3F_SHIFT_FSK_RX_FINISHED)
#define BK4819_REG_3F_MASK_FSK_FIFO_ALMOST_FULL                                \
  (1U << BK4819_REG_3F_SHIFT_FSK_FIFO_ALMOST_FULL)
#define BK4819_REG_3F_MASK_DTMF_5TONE_FOUND                                    \
  (1U << BK4819_REG_3F_SHIFT_DTMF_5TONE_FOUND)
#define BK4819_REG_3F_MASK_CxCSS_TAIL (1U << BK4819_REG_3F_SHIFT_CxCSS_TAIL)
#define BK4819_REG_3F_MASK_CDCSS_FOUND (1U << BK4819_REG_3F_SHIFT_CDCSS_FOUND)
#define BK4819_REG_3F_MASK_CDCSS_LOST (1U << BK4819_REG_3F_SHIFT_CDCSS_LOST)
#define BK4819_REG_3F_MASK_CTCSS_FOUND (1U << BK4819_REG_3F_SHIFT_CTCSS_FOUND)
#define BK4819_REG_3F_MASK_CTCSS_LOST (1U << BK4819_REG_3F_SHIFT_CTCSS_LOST)
#define BK4819_REG_3F_MASK_VOX_FOUND (1U << BK4819_REG_3F_SHIFT_VOX_FOUND)
#define BK4819_REG_3F_MASK_VOX_LOST (1U << BK4819_REG_3F_SHIFT_VOX_LOST)
#define BK4819_REG_3F_MASK_SQUELCH_FOUND                                       \
  (1U << BK4819_REG_3F_SHIFT_SQUELCH_FOUND)
#define BK4819_REG_3F_MASK_SQUELCH_LOST (1U << BK4819_REG_3F_SHIFT_SQUELCH_LOST)
#define BK4819_REG_3F_MASK_FSK_RX_SYNC (1U << BK4819_REG_3F_SHIFT_FSK_RX_SYNC)

#define BK4819_REG_3F_FSK_TX_FINISHED                                          \
  (1U << BK4819_REG_3F_SHIFT_FSK_TX_FINISHED)
#define BK4819_REG_3F_FSK_FIFO_ALMOST_EMPTY                                    \
  (1U << BK4819_REG_3F_SHIFT_FSK_FIFO_ALMOST_EMPTY)
#define BK4819_REG_3F_FSK_RX_FINISHED                                          \
  (1U << BK4819_REG_3F_SHIFT_FSK_RX_FINISHED)
#define BK4819_REG_3F_FSK_FIFO_ALMOST_FULL                                     \
  (1U << BK4819_REG_3F_SHIFT_FSK_FIFO_ALMOST_FULL)
#define BK4819_REG_3F_DTMF_5TONE_FOUND                                         \
  (1U << BK4819_REG_3F_SHIFT_DTMF_5TONE_FOUND)
#define BK4819_REG_3F_CxCSS_TAIL (1U << BK4819_REG_3F_SHIFT_CxCSS_TAIL)
#define BK4819_REG_3F_CDCSS_FOUND (1U << BK4819_REG_3F_SHIFT_CDCSS_FOUND)
#define BK4819_REG_3F_CDCSS_LOST (1U << BK4819_REG_3F_SHIFT_CDCSS_LOST)
#define BK4819_REG_3F_CTCSS_FOUND (1U << BK4819_REG_3F_SHIFT_CTCSS_FOUND)
#define BK4819_REG_3F_CTCSS_LOST (1U << BK4819_REG_3F_SHIFT_CTCSS_LOST)
#define BK4819_REG_3F_VOX_FOUND (1U << BK4819_REG_3F_SHIFT_VOX_FOUND)
#define BK4819_REG_3F_VOX_LOST (1U << BK4819_REG_3F_SHIFT_VOX_LOST)
#define BK4819_REG_3F_SQUELCH_FOUND (1U << BK4819_REG_3F_SHIFT_SQUELCH_FOUND)
#define BK4819_REG_3F_SQUELCH_LOST (1U << BK4819_REG_3F_SHIFT_SQUELCH_LOST)
#define BK4819_REG_3F_FSK_RX_SYNC (1U << BK4819_REG_3F_SHIFT_FSK_RX_SYNC)

// REG 51

#define BK4819_REG_51_SHIFT_ENABLE_CxCSS 15
#define BK4819_REG_51_SHIFT_GPIO6_PIN2_INPUT 14
#define BK4819_REG_51_SHIFT_TX_CDCSS_POLARITY 13
#define BK4819_REG_51_SHIFT_CxCSS_MODE 12
#define BK4819_REG_51_SHIFT_CDCSS_BIT_WIDTH 11
#define BK4819_REG_51_SHIFT_1050HZ_DETECTION 10
#define BK4819_REG_51_SHIFT_AUTO_CDCSS_BW 9
#define BK4819_REG_51_SHIFT_AUTO_CTCSS_BW 8
#define BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1 0

#define BK4819_REG_51_MASK_ENABLE_CxCSS                                        \
  (0x01U << BK4819_REG_51_SHIFT_ENABLE_CxCSS)
#define BK4819_REG_51_MASK_GPIO6_PIN2_INPUT                                    \
  (0x01U << BK4819_REG_51_SHIFT_GPIO6_PIN2_INPUT)
#define BK4819_REG_51_MASK_TX_CDCSS_POLARITY                                   \
  (0x01U << BK4819_REG_51_SHIFT_TX_CDCSS_POLARITY)
#define BK4819_REG_51_MASK_CxCSS_MODE (0x01U << BK4819_REG_51_SHIFT_CxCSS_MODE)
#define BK4819_REG_51_MASK_CDCSS_BIT_WIDTH                                     \
  (0x01U << BK4819_REG_51_SHIFT_CDCSS_BIT_WIDTH)
#define BK4819_REG_51_MASK_1050HZ_DETECTION                                    \
  (0x01U << BK4819_REG_51_SHIFT_1050HZ_DETECTION)
#define BK4819_REG_51_MASK_AUTO_CDCSS_BW                                       \
  (0x01U << BK4819_REG_51_SHIFT_AUTO_CDCSS_BW)
#define BK4819_REG_51_MASK_AUTO_CTCSS_BW                                       \
  (0x01U << BK4819_REG_51_SHIFT_AUTO_CTCSS_BW)
#define BK4819_REG_51_MASK_CxCSS_TX_GAIN1                                      \
  (0x7FU << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1)

enum {
  BK4819_REG_51_ENABLE_CxCSS = (1U << BK4819_REG_51_SHIFT_ENABLE_CxCSS),
  BK4819_REG_51_DISABLE_CxCSS = (0U << BK4819_REG_51_SHIFT_ENABLE_CxCSS),

  BK4819_REG_51_GPIO6_PIN2_INPUT = (1U << BK4819_REG_51_SHIFT_GPIO6_PIN2_INPUT),
  BK4819_REG_51_GPIO6_PIN2_NORMAL =
      (0U << BK4819_REG_51_SHIFT_GPIO6_PIN2_INPUT),

  BK4819_REG_51_TX_CDCSS_NEGATIVE =
      (1U << BK4819_REG_51_SHIFT_TX_CDCSS_POLARITY),
  BK4819_REG_51_TX_CDCSS_POSITIVE =
      (0U << BK4819_REG_51_SHIFT_TX_CDCSS_POLARITY),

  BK4819_REG_51_MODE_CTCSS = (1U << BK4819_REG_51_SHIFT_CxCSS_MODE),
  BK4819_REG_51_MODE_CDCSS = (0U << BK4819_REG_51_SHIFT_CxCSS_MODE),

  BK4819_REG_51_CDCSS_24_BIT = (1U << BK4819_REG_51_SHIFT_CDCSS_BIT_WIDTH),
  BK4819_REG_51_CDCSS_23_BIT = (0U << BK4819_REG_51_SHIFT_CDCSS_BIT_WIDTH),

  BK4819_REG_51_1050HZ_DETECTION = (1U << BK4819_REG_51_SHIFT_1050HZ_DETECTION),
  BK4819_REG_51_1050HZ_NO_DETECTION =
      (0U << BK4819_REG_51_SHIFT_1050HZ_DETECTION),

  BK4819_REG_51_AUTO_CDCSS_BW_DISABLE =
      (1U << BK4819_REG_51_SHIFT_AUTO_CDCSS_BW),
  BK4819_REG_51_AUTO_CDCSS_BW_ENABLE =
      (0U << BK4819_REG_51_SHIFT_AUTO_CDCSS_BW),

  BK4819_REG_51_AUTO_CTCSS_BW_DISABLE =
      (1U << BK4819_REG_51_SHIFT_AUTO_CTCSS_BW),
  BK4819_REG_51_AUTO_CTCSS_BW_ENABLE =
      (0U << BK4819_REG_51_SHIFT_AUTO_CTCSS_BW),
};

// REG 70

#define BK4819_REG_70_SHIFT_ENABLE_TONE1 15
#define BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN 8
#define BK4819_REG_70_SHIFT_ENABLE_TONE2 7
#define BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN 0

#define BK4819_REG_70_MASK_ENABLE_TONE1                                        \
  (0x01U << BK4819_REG_70_SHIFT_ENABLE_TONE1)
#define BK4819_REG_70_MASK_TONE1_TUNING_GAIN                                   \
  (0x7FU << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN)
#define BK4819_REG_70_MASK_ENABLE_TONE2                                        \
  (0x01U << BK4819_REG_70_SHIFT_ENABLE_TONE2)
#define BK4819_REG_70_MASK_TONE2_TUNING_GAIN                                   \
  (0x7FU << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN)

enum {
  BK4819_REG_70_ENABLE_TONE1 = (1U << BK4819_REG_70_SHIFT_ENABLE_TONE1),
  BK4819_REG_70_ENABLE_TONE2 = (1U << BK4819_REG_70_SHIFT_ENABLE_TONE2),
};

#endif
