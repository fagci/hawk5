#include "../inc/dp32g030/uart.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../external/printf/printf.h"
#include "../inc/dp32g030/dma.h"
#include "../inc/dp32g030/gpio.h"
#include "../inc/dp32g030/syscon.h"
#include "../scheduler.h"
#include "bk4819-regs.h"
#include "bk4819.h"
#include "crc.h"
#include "eeprom.h"
#include "gpio.h"
#include "uart.h"
#include <stdbool.h>
#include <string.h>

static const char Version[] = "s0v4";

static uint8_t UART_DMA_Buffer[256];

static bool bIsInLockScreen = false;

// UART protocol magic bytes
enum {
  UART_MAGIC_START_1 = 0xAB,
  UART_MAGIC_START_2 = 0xCD,
  UART_MAGIC_END_1 = 0xDC,
  UART_MAGIC_END_2 = 0xBA,
  UART_MAGIC_HEADER = 0xCDAB,
  UART_MAGIC_FOOTER = 0xBADC
};

void UART_Init(void) {
  uint32_t Delta;
  uint32_t Positive;
  uint32_t Frequency;

  UART1->CTRL =
      (UART1->CTRL & ~UART_CTRL_UARTEN_MASK) | UART_CTRL_UARTEN_BITS_DISABLE;
  Delta = SYSCON_RC_FREQ_DELTA;
  Positive = (Delta & SYSCON_RC_FREQ_DELTA_RCHF_SIG_MASK) >>
             SYSCON_RC_FREQ_DELTA_RCHF_SIG_SHIFT;
  Frequency = (Delta & SYSCON_RC_FREQ_DELTA_RCHF_DELTA_MASK) >>
              SYSCON_RC_FREQ_DELTA_RCHF_DELTA_SHIFT;
  if (Positive) {
    Frequency += 48000000U;
  } else {
    Frequency = 48000000U - Frequency;
  }

  UART1->BAUD = Frequency / 39053U;
  UART1->CTRL = UART_CTRL_RXEN_BITS_ENABLE | UART_CTRL_TXEN_BITS_ENABLE |
                UART_CTRL_RXDMAEN_BITS_ENABLE;
  UART1->RXTO = 4;
  UART1->FC = 0;
  UART1->FIFO = UART_FIFO_RF_LEVEL_BITS_8_BYTE | UART_FIFO_RF_CLR_BITS_ENABLE |
                UART_FIFO_TF_CLR_BITS_ENABLE;
  UART1->IE = 0;

  DMA_CTR = (DMA_CTR & ~DMA_CTR_DMAEN_MASK) | DMA_CTR_DMAEN_BITS_DISABLE;

  DMA_CH0->MSADDR = (uint32_t)(uintptr_t)&UART1->RDR;
  DMA_CH0->MDADDR = (uint32_t)(uintptr_t)UART_DMA_Buffer;
  DMA_CH0->MOD = 0
                 // Source
                 | DMA_CH_MOD_MS_ADDMOD_BITS_NONE |
                 DMA_CH_MOD_MS_SIZE_BITS_8BIT |
                 DMA_CH_MOD_MS_SEL_BITS_HSREQ_MS1
                 // Destination
                 | DMA_CH_MOD_MD_ADDMOD_BITS_INCREMENT |
                 DMA_CH_MOD_MD_SIZE_BITS_8BIT | DMA_CH_MOD_MD_SEL_BITS_SRAM;
  DMA_INTEN = 0;
  DMA_INTST =
      0 | DMA_INTST_CH0_TC_INTST_BITS_SET | DMA_INTST_CH1_TC_INTST_BITS_SET |
      DMA_INTST_CH2_TC_INTST_BITS_SET | DMA_INTST_CH3_TC_INTST_BITS_SET |
      DMA_INTST_CH0_THC_INTST_BITS_SET | DMA_INTST_CH1_THC_INTST_BITS_SET |
      DMA_INTST_CH2_THC_INTST_BITS_SET | DMA_INTST_CH3_THC_INTST_BITS_SET;
  DMA_CH0->CTR = 0 | DMA_CH_CTR_CH_EN_BITS_ENABLE |
                 ((0xFF << DMA_CH_CTR_LENGTH_SHIFT) & DMA_CH_CTR_LENGTH_MASK) |
                 DMA_CH_CTR_LOOP_BITS_ENABLE | DMA_CH_CTR_PRI_BITS_MEDIUM;
  UART1->IF = UART_IF_RXTO_BITS_SET;

  DMA_CTR = (DMA_CTR & ~DMA_CTR_DMAEN_MASK) | DMA_CTR_DMAEN_BITS_ENABLE;

  UART1->CTRL |= UART_CTRL_UARTEN_BITS_ENABLE;
}

void UART_Send(const void *pBuffer, uint32_t Size) {
  const uint8_t *pData = (const uint8_t *)pBuffer;
  uint32_t i;

  for (i = 0; i < Size; i++) {
    UART1->TDR = pData[i];
    while ((UART1->IF & UART_IF_TXFIFO_FULL_MASK) !=
           UART_IF_TXFIFO_FULL_BITS_NOT_SET) {
    }
  }
}

static inline uint16_t DMA_INDEX(uint16_t x, uint16_t y) {
  return (x + y) % sizeof(UART_DMA_Buffer);
}

typedef struct {
  uint16_t ID;
  uint16_t Size;
} Header_t;

typedef struct {
  uint8_t Padding[2];
  uint16_t ID;
} Footer_t;

typedef struct {
  Header_t Header;
  uint32_t Timestamp;
} CMD_0514_t;

typedef struct {
  Header_t Header;
  struct {
    char Version[16];
    bool bHasCustomAesKey;
    bool bIsInLockScreen;
    uint8_t Padding[2];
    uint32_t Challenge[4];
  } Data;
} REPLY_0514_t;

typedef struct {
  Header_t Header;
  uint32_t Offset;
  uint8_t Size;
  uint8_t Padding[3];
  uint32_t Timestamp;
} CMD_051B_t;

typedef struct {
  Header_t Header;
  struct {
    uint32_t Offset;
    uint8_t Size;
    uint8_t Padding[3];
    uint8_t Data[128];
  } Data;
} REPLY_051B_t;

typedef struct {
  Header_t Header;
  uint32_t Offset;
  uint8_t Size;
  uint8_t Padding[2];
  bool bAllowPassword;
  uint32_t Timestamp;
  uint8_t Data[128];
} CMD_051D_t;

typedef struct {
  Header_t Header;
  struct {
    uint32_t Offset;
    uint8_t Padding[2];
  } Data;
} REPLY_051D_t;

typedef struct {
  Header_t Header;
  struct {
    uint16_t RSSI;
    uint8_t ExNoiseIndicator;
    uint8_t GlitchIndicator;
  } Data;
} REPLY_0527_t;

typedef struct {
  Header_t Header;
  uint32_t Response[4];
} CMD_052D_t;

typedef struct {
  Header_t Header;
  struct {
    bool bIsLocked;
    uint8_t Padding[3];
  } Data;
} REPLY_052D_t;

typedef struct {
  Header_t Header;
  uint32_t Timestamp;
} CMD_052F_t;

static const uint8_t Obfuscation[16] = {0x16, 0x6C, 0x14, 0xE6, 0x2E, 0x91,
                                        0x0D, 0x40, 0x21, 0x35, 0xD5, 0x40,
                                        0x13, 0x03, 0xE9, 0x80};

static union {
  uint8_t Buffer[256];
  struct {
    Header_t Header;
    uint8_t Data[252];
  };
} UART_Command;

static uint32_t Timestamp;
static uint16_t gUART_WriteIndex;
static bool bIsEncrypted = true;

static Header_t Header;
static Footer_t Footer;

// Helper function for XOR obfuscation
static void XorObfuscate(uint8_t *data, uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
    data[i] ^= Obfuscation[i % 16];
  }
}

static void SendReply(void *pReply, uint16_t Size) {
  if (bIsEncrypted) {
    XorObfuscate((uint8_t *)pReply, Size);
  }

  Header.ID = UART_MAGIC_HEADER;
  Header.Size = Size;
  UART_Send(&Header, sizeof(Header));
  UART_Send(pReply, Size);
  
  Footer.Padding[0] = bIsEncrypted ? (Obfuscation[Size % 16] ^ 0xFF) : 0xFF;
  Footer.Padding[1] = bIsEncrypted ? (Obfuscation[(Size + 1) % 16] ^ 0xFF) : 0xFF;
  Footer.ID = UART_MAGIC_FOOTER;

  UART_Send(&Footer, sizeof(Footer));
}

static void SendVersion(void) {
  REPLY_0514_t Reply;

  Reply.Header.ID = 0x0515;
  Reply.Header.Size = sizeof(Reply.Data);
  strcpy(Reply.Data.Version, Version);
  Reply.Data.bHasCustomAesKey = false;
  Reply.Data.bIsInLockScreen = bIsInLockScreen;

  SendReply(&Reply, sizeof(Reply));
}

// Combined handler for 0x0514 and 0x052F (same logic)
static void CMD_051X_VersionRequest(const uint8_t *pBuffer) {
  const CMD_0514_t *pCmd = (const CMD_0514_t *)pBuffer;

  Timestamp = pCmd->Timestamp;
  GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
  SendVersion();
}

static void CMD_051B(const uint8_t *pBuffer) {
  const CMD_051B_t *pCmd = (const CMD_051B_t *)pBuffer;
  REPLY_051B_t Reply;

  if (pCmd->Timestamp != Timestamp) {
    return;
  }

  memset(&Reply, 0, sizeof(Reply));
  Reply.Header.ID = 0x051C;
  Reply.Header.Size = pCmd->Size + 8 + 4;
  Reply.Data.Offset = pCmd->Offset;
  Reply.Data.Size = pCmd->Size;

  EEPROM_ReadBuffer(pCmd->Offset, Reply.Data.Data, pCmd->Size);

  SendReply(&Reply, pCmd->Size + 8 + 4);
}

static void CMD_051D(const uint8_t *pBuffer) {
  const CMD_051D_t *pCmd = (const CMD_051D_t *)pBuffer;
  REPLY_051D_t Reply;

  if (pCmd->Timestamp != Timestamp) {
    return;
  }

  Reply.Header.ID = 0x051E;
  Reply.Header.Size = sizeof(Reply.Data);
  Reply.Data.Offset = pCmd->Offset;

  uint16_t i;

  for (i = 0; i < (pCmd->Size / 8U); i++) {
    uint32_t Offset = pCmd->Offset + (i * 8U);

    if ((Offset < 0x0E98 || Offset >= 0x0EA0) || !bIsInLockScreen ||
        pCmd->bAllowPassword) {
      EEPROM_WriteBuffer(Offset, (void *)&pCmd->Data[i * 8U], 8);
    }
  }

  SendReply(&Reply, sizeof(Reply));
}

static void CMD_0527(void) {
  REPLY_0527_t Reply;

  Reply.Header.ID = 0x0528;
  Reply.Header.Size = sizeof(Reply.Data);
  Reply.Data.RSSI = BK4819_ReadRegister(BK4819_REG_67) & 0x01FF;
  Reply.Data.ExNoiseIndicator = BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
  Reply.Data.GlitchIndicator = BK4819_ReadRegister(BK4819_REG_63);

  SendReply(&Reply, sizeof(Reply));
}

static void CMD_052D(const uint8_t *pBuffer) {
  (void)pBuffer;
  REPLY_052D_t Reply;

  Reply.Header.ID = 0x052E;
  Reply.Header.Size = sizeof(Reply.Data);

  Reply.Data.bIsLocked = false;
  SendReply(&Reply, sizeof(Reply));
}

bool UART_IsCommandAvailable(void) {
  uint16_t DmaLength;
  uint16_t CommandLength;
  uint16_t Index;
  uint16_t TailIndex;
  uint16_t Size;
  uint16_t CRC;

  DmaLength = DMA_CH0->ST & 0xFFFU;
  while (1) {
    if (gUART_WriteIndex == DmaLength) {
      return false;
    }

    while (gUART_WriteIndex != DmaLength &&
           UART_DMA_Buffer[gUART_WriteIndex] != UART_MAGIC_START_1) {
      gUART_WriteIndex = DMA_INDEX(gUART_WriteIndex, 1);
    }

    if (gUART_WriteIndex == DmaLength) {
      return false;
    }

    if (gUART_WriteIndex < DmaLength) {
      CommandLength = DmaLength - gUART_WriteIndex;
    } else {
      CommandLength = (DmaLength + sizeof(UART_DMA_Buffer)) - gUART_WriteIndex;
    }
    if (CommandLength < 8u) {
      return 0;
    }
    if (UART_DMA_Buffer[DMA_INDEX(gUART_WriteIndex, 1)] == UART_MAGIC_START_2) {
      break;
    }
    gUART_WriteIndex = DMA_INDEX(gUART_WriteIndex, 1);
  }

  Index = DMA_INDEX(gUART_WriteIndex, 2);
  Size = (UART_DMA_Buffer[DMA_INDEX(Index, 1)] << 8) | UART_DMA_Buffer[Index];
  if (Size + 8U > sizeof(UART_DMA_Buffer)) {
    gUART_WriteIndex = DmaLength;
    return false;
  }
  if (CommandLength < Size + 8) {
    return false;
  }
  Index = DMA_INDEX(Index, 2);
  TailIndex = DMA_INDEX(Index, Size + 2);
  if (UART_DMA_Buffer[TailIndex] != UART_MAGIC_END_1 ||
      UART_DMA_Buffer[DMA_INDEX(TailIndex, 1)] != UART_MAGIC_END_2) {
    gUART_WriteIndex = DmaLength;
    return false;
  }
  if (TailIndex < Index) {
    uint16_t ChunkSize = sizeof(UART_DMA_Buffer) - Index;

    memcpy(UART_Command.Buffer, UART_DMA_Buffer + Index, ChunkSize);
    memcpy(UART_Command.Buffer + ChunkSize, UART_DMA_Buffer, TailIndex);
  } else {
    memcpy(UART_Command.Buffer, UART_DMA_Buffer + Index, TailIndex - Index);
  }

  TailIndex = DMA_INDEX(TailIndex, 2);
  if (TailIndex < gUART_WriteIndex) {
    memset(UART_DMA_Buffer + gUART_WriteIndex, 0,
           sizeof(UART_DMA_Buffer) - gUART_WriteIndex);
    memset(UART_DMA_Buffer, 0, TailIndex);
  } else {
    memset(UART_DMA_Buffer + gUART_WriteIndex, 0, TailIndex - gUART_WriteIndex);
  }

  gUART_WriteIndex = TailIndex;

  if (UART_Command.Header.ID == 0x0514) {
    bIsEncrypted = false;
  }
  if (UART_Command.Header.ID == 0x6902) {
    bIsEncrypted = true;
  }

  if (bIsEncrypted) {
    XorObfuscate(UART_Command.Buffer, Size + 2);
  }

  CRC = UART_Command.Buffer[Size] | (UART_Command.Buffer[Size + 1] << 8);
  if (CRC_Calculate(UART_Command.Buffer, Size) != CRC) {
    return false;
  }

  return true;
}

void UART_HandleCommand(void) {
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, true);
  switch (UART_Command.Header.ID) {
  case 0x0514:
  case 0x052F:
    CMD_051X_VersionRequest(UART_Command.Buffer);
    break;

  case 0x051B:
    CMD_051B(UART_Command.Buffer);
    break;

  case 0x051D:
    CMD_051D(UART_Command.Buffer);
    break;

  case 0x0527:
    CMD_0527();
    break;

  case 0x052D:
    CMD_052D(UART_Command.Buffer);
    break;

  case 0x05DD:
    NVIC_SystemReset();
    break;
  }
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, false);
}

void LogUart(const char *const str) { UART_Send(str, strlen(str)); }

// #define DEBUG 1

#ifdef DEBUG
void Log(const char *pattern, ...) {
  char text[128];
  va_list args;
  va_start(args, pattern);
  vsnprintf(text, sizeof(text), pattern, args);
  va_end(args);
  printf("%+10u %s\n", Now(), text);
}
void LogC(LogColor c, const char *pattern, ...) {
  char text[128];
  va_list args;
  va_start(args, pattern);
  vsnprintf(text, sizeof(text), pattern, args);
  va_end(args);
  printf("%+10u \033[%um%s\033[%um\n", Now(), c, text, LOG_C_RESET);
}
#else
void Log(const char *pattern, ...) { (void)pattern; }
void LogC(LogColor c, const char *pattern, ...) {
  (void)c;
  (void)pattern;
}
#endif
