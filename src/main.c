#include "board.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "inc/dp32g030/gpio.h"
#include "system.h"

void Main(void) {
  SYS_ConfigureClocks();
  SYS_ConfigureSysCon();

  BOARD_GPIO_Init();
  BOARD_PORTCON_Init();
  BOARD_ADC_Init();

  CRC_Init();
  UART_Init();
  I2C_Init();

  TIMER0_InitAsUptimeCounter();
  TIMER1_InitForDelay();
  /* for (;;) {
    printf("Tick %u\n", (uint32_t)GetUptimeMs());
    for (int i = 0; i < 10; ++i)
      TIMER_DelayUs(1000);
  } */

  EEPROM_Init(); // Автоматически определит параметры
  EEPROM_TestReadSpeedFast();
  for (;;) {
  }
  /* EEPROM_ScanBus();
  for (;;) {
  } */

  printf("hawk5\n");

  SYS_Main();
}
