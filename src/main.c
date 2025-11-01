#include "board.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "system.h"

void Main(void) {
  SYS_ConfigureSysCon();
  SYS_ConfigureClocks();
  TIM0_INIT();

  BOARD_GPIO_Init();
  BOARD_PORTCON_Init();
  BOARD_ADC_Init();

  CRC_Init();
  UART_Init();

  LogC(LOG_C_BRIGHT_YELLOW, "hawk5");

  SYS_Main();
}
