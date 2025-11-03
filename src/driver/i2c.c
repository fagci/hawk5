#include "i2c.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../external/printf/printf.h"
#include "../inc/dp32g030/gpio.h"
#include "../inc/dp32g030/portcon.h"
#include "gpio.h"
#include "systick.h"

static inline void i2c_delay(void) {
  for (volatile uint32_t i = 0; i < 2; i++) {
    __NOP();
  }
}

/* === Управление состоянием SDA === */

/* SDA -> Output (для записи) */
static inline void sda_out(void) {
  PORTCON_PORTA_IE &= ~PORTCON_PORTA_IE_A11_MASK;
  PORTCON_PORTA_OD |= PORTCON_PORTA_OD_A11_BITS_ENABLE;
  GPIOA->DIR |= GPIO_DIR_11_BITS_OUTPUT;
}

/* SDA -> Input (для чтения) */
static inline void sda_in(void) {
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA); // Отпускаем линию
  PORTCON_PORTA_IE |= PORTCON_PORTA_IE_A11_BITS_ENABLE;
  GPIOA->DIR &= ~GPIO_DIR_11_MASK;
}

/* Установить SDA = 0 */
static inline void sda_lo(void) {
  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
}

/* Установить SDA = 1 */
static inline void sda_hi(void) {
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
}

/* Прочитать SDA */
static inline bool sda_read(void) {
  return GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
}

/* === Управление SCL === */

static inline void scl_lo(void) {
  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
}

static inline void scl_hi(void) {
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
}

/* === Основные функции I2C === */

void I2C_Init(void) {
  /* Open-drain для обеих линий */
  PORTCON_PORTA_OD |=
      PORTCON_PORTA_OD_A10_BITS_ENABLE | PORTCON_PORTA_OD_A11_BITS_ENABLE;

  /* Оба пина как выходы */
  GPIOA->DIR |= GPIO_DIR_10_BITS_OUTPUT | GPIO_DIR_11_BITS_OUTPUT;

  /* Idle state: обе линии HIGH */
  scl_hi();
  sda_hi();

  TIMER_DelayUs(10);
  // printf("I2C initialized\n");
}

void I2C_Start(void) {
  /* Убедимся что SDA в режиме выхода */
  sda_out();

  /* START condition:
     1. Обе линии HIGH (idle)
     2. SDA падает при SCL=HIGH
     3. SCL падает */

  sda_hi();
  scl_hi();
  i2c_delay();

  sda_lo(); // SDA падает при SCL=HIGH
  i2c_delay();

  scl_lo(); // Готовы к передаче данных
  i2c_delay();
}

void I2C_RepStart(void) {
  /* Repeated START (предполагается SCL=LOW):
     1. SDA поднимается при SCL=LOW
     2. SCL поднимается
     3. SDA падает при SCL=HIGH (START)
     4. SCL падает */

  sda_out();

  sda_hi(); // SDA HIGH при SCL=LOW
  i2c_delay();

  scl_hi(); // SCL поднимается
  i2c_delay();

  sda_lo(); // START: SDA падает при SCL=HIGH
  i2c_delay();

  scl_lo(); // Готовы к передаче
  i2c_delay();
}

void I2C_Stop(void) {
  /* STOP condition (предполагается SCL=LOW):
     1. SDA=LOW при SCL=LOW
     2. SCL поднимается
     3. SDA поднимается при SCL=HIGH (STOP) */

  sda_out();

  sda_lo(); // Убедимся что SDA=LOW
  i2c_delay();

  scl_hi(); // SCL поднимается
  i2c_delay();

  sda_hi(); // STOP: SDA поднимается при SCL=HIGH
  i2c_delay();
}

int I2C_Write(uint8_t Data) {
  int ret = -1;

  sda_out();
  scl_lo();
  i2c_delay();

  /* Передаём 8 бит (MSB first) */
  for (uint8_t i = 0; i < 8; i++) {
    if (Data & 0x80) {
      sda_hi();
    } else {
      sda_lo();
    }
    Data <<= 1;
    i2c_delay();

    scl_hi(); // Данные считываются на подъёме SCL
    i2c_delay();
    scl_lo();
  }

  /* Читаем ACK от slave:
     1. Отпускаем SDA (master -> input)
     2. SCL поднимается
     3. Читаем SDA (0=ACK, 1=NACK)
     4. SCL падает */

  sda_in();
  i2c_delay();

  scl_hi();
  i2c_delay();
  i2c_delay(); // Дополнительная задержка для стабилизации

  if (!sda_read()) {
    ret = 0; // ACK получен
  }

  scl_lo();
  i2c_delay();

  /* Возвращаем SDA в режим выхода */
  sda_out();
  sda_lo();

  return ret;
}

uint8_t I2C_Read(bool bFinal) {
  uint8_t Data = 0;

  /* Переключаем SDA на вход для чтения от slave */
  sda_in();

  /* Читаем 8 бит (MSB first) */
  for (uint8_t i = 0; i < 8; i++) {
    scl_lo();
    i2c_delay();

    scl_hi();
    i2c_delay();

    Data <<= 1;
    if (sda_read()) {
      Data |= 1;
    }
  }

  scl_lo();
  i2c_delay();

  /* Отправляем ACK/NACK:
     1. SDA -> output
     2. SDA = 0 (ACK) или 1 (NACK)
     3. SCL поднимается
     4. SCL падает */

  sda_out();

  if (bFinal) {
    sda_hi(); // NACK (последний байт)
  } else {
    sda_lo(); // ACK (продолжаем чтение)
  }

  i2c_delay();
  scl_hi();
  i2c_delay();
  scl_lo();
  i2c_delay();

  return Data;
}

/* === Вспомогательные функции === */

void I2C_SimpleTest(void) {
  printf("I2C simple test:\n");

  // Тест 1: START + адрес + STOP
  I2C_Start();
  int result = I2C_Write(0xA0); // EEPROM write address
  I2C_Stop();

  printf("  Write 0xA0: %s\n", result == 0 ? "ACK" : "NACK");

  TIMER_DelayUs(1000);

  // Тест 2: Сканируем адреса
  for (uint8_t addr = 0xA0; addr <= 0xAE; addr += 2) {
    I2C_Start();
    result = I2C_Write(addr);
    I2C_Stop();

    if (result == 0) {
      printf("  Found device at 0x%02X\n", addr);
    }

    TIMER_DelayUs(1000);
  }
}

uint16_t I2C_ReadBuffer(void *pBuffer, uint16_t Size) {
  uint8_t *pData = (uint8_t *)pBuffer;

  if (Size == 0)
    return 0;

  for (uint16_t i = 0; i < Size - 1; i++) {
    pData[i] = I2C_Read(false); // ACK
  }

  pData[Size - 1] = I2C_Read(true); // NACK на последнем байте

  return Size;
}

/* int I2C_WriteBuffer(const uint8_t *pBuffer, uint16_t Size) {
  for (uint16_t i = 0; i < Size; i++) {
    if (I2C_Write(pBuffer[i]) != 0) {
      printf("I2C_WriteBuffer: NACK at byte %u\n", i);
      return -1;
    }
  }
  return 0;
} */
int I2C_WriteBuffer(const uint8_t *pBuffer, uint16_t Size) {
  for (uint16_t i = 0; i < Size; i++) {
    int retry = 3; // 3 попытки
    while (retry--) {
      if (I2C_Write(pBuffer[i]) == 0) {
        break; // ACK - успех
      }
      if (retry > 0) {
        // printf("I2C_WriteBuffer: NACK at byte %u, retry %d\n", i, 3 - retry);
        i2c_delay();
      } else {
        // printf("I2C_WriteBuffer: NACK at byte %u - FAILED\n", i);
        return -1;
      }
    }
  }
  return 0;
}
