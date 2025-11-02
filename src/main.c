#include "board.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "system.h"

void Main(void) {
  SYS_ConfigureClocks();
  SYS_ConfigureSysCon();

  TIMER0_InitAsUptimeCounter();
  TIMER1_InitForDelay();

  BOARD_GPIO_Init();
  BOARD_PORTCON_Init();
  BOARD_ADC_Init();

  CRC_Init();
  UART_Init();

  printf("hawk5\n");

  SYS_Main();
}
