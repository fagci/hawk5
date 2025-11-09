#include "i2c.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../external/printf/printf.h"
#include "../inc/dp32g030/gpio.h"
#include "../inc/dp32g030/portcon.h"
#include "gpio.h"
#include "systick.h"

static inline void i2c_delay_short(void) {
  __asm volatile("nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n");
  // TIMER_DelayTicks(2);
}

static inline void i2c_delay_long(void) {
  __asm volatile("nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n nop\n nop\n"
                 "nop\n nop\n nop\n");
  // TIMER_DelayTicks(4);
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

static inline bool scl_read(void) {
  return GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
}

/* === Управление SCL === */

static inline void scl_lo(void) {
  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
}

static inline void scl_hi(void) {
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
}

static inline bool scl_hi_wait(void) {
  scl_hi();

  uint32_t timeout = 10000;
  while (!scl_read() && timeout--) {
    TIMER_DelayUs(1);
  }
  return timeout > 0;
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
}

void I2C_Start(void) {
  sda_out();
  sda_hi();
  i2c_delay_short(); // Setup time для START (600 нс)
  sda_lo();          // SDA падает при SCL=HIGH
  i2c_delay_short(); // Hold time для START (600 нс)
  scl_lo();
  i2c_delay_long();
}

void I2C_Stop(void) {
  sda_out();
  sda_lo();
  i2c_delay_long();
  scl_hi();
  i2c_delay_short(); // Setup time для STOP (600 нс)
  sda_hi();          // SDA поднимается при SCL=HIGH
  i2c_delay_long();  // Bus free time
}

void I2C_RepStart(void) {
  sda_out();
  scl_lo(); // Убедимся что SCL = LOW
  i2c_delay_long();
  sda_hi(); // SDA поднимается при LOW SCL
  i2c_delay_short();
  scl_hi(); // SCL поднимается
  i2c_delay_short();
  sda_lo(); // SDA падает при HIGH SCL = Repeated START
  i2c_delay_short();
  scl_lo();
  i2c_delay_long();
}

int I2C_Write(uint8_t Data) {
  int ret = -1;
  sda_out();
  scl_lo();
  i2c_delay_long(); // Добавить задержку LOW периода

  for (uint8_t i = 0; i < 8; i++) {
    ((Data & 0x80) ? sda_hi : sda_lo)();
    Data <<= 1;

    i2c_delay_short(); // Setup time для данных (100 нс минимум)
    scl_hi(); // Данные считываются на подъёме SCL
    i2c_delay_short(); // HIGH период SCL
    scl_lo();
    i2c_delay_long(); // LOW период SCL
  }

  // Читаем ACK
  sda_in();
  scl_hi_wait();
  i2c_delay_short();

  if (!sda_read()) {
    ret = 0; // ACK получен
  }

  scl_lo();
  // sda_out();
  // sda_lo();
  return ret;
}

uint8_t I2C_Read(bool bFinal) {
  uint8_t Data = 0;
  sda_in();

  for (uint8_t i = 0; i < 8; i++) {
    Data <<= 1;

    scl_hi();
    i2c_delay_short();

    if (sda_read()) {
      Data |= 1;
    }
    i2c_delay_short(); // tHIGH

    scl_lo();
    i2c_delay_long();
  }

  // ACK/NACK
  sda_out();
  (bFinal ? sda_hi : sda_lo)();

  scl_hi_wait();

  i2c_delay_short();
  scl_lo();

  return Data;
}

/* === Вспомогательные функции === */

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

int I2C_WriteBuffer(const uint8_t *pBuffer, uint16_t Size) {
  for (uint16_t i = 0; i < Size; i++) {
    int retry = 3; // 3 попытки
    while (retry--) {
      if (I2C_Write(pBuffer[i]) == 0) {
        break; // ACK - успех
      }
      if (retry > 0) {
        // printf("I2C_WriteBuffer: NACK at byte %u, retry %d\n", i, 3 - retry);
        i2c_delay_short();
      } else {
        // printf("I2C_WriteBuffer: NACK at byte %u - FAILED\n", i);
        return -1;
      }
    }
  }
  return 0;
}
