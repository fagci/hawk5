#include "board.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "inc/dp32g030/uart.h"
#include "system.h"

int fputc(int ch, void *f) {
  (void)f;
  while (UART1->IF & UART_IF_TXBUSY_MASK)
    ;

  UART1->TDR = (uint8_t)ch;

  return ch;
}

void Main(void) {
  SYS_ConfigureSysCon();
  SYS_ConfigureClocks();

  TIM0_INIT();
  TIM1_INIT();

  BOARD_GPIO_Init();
  BOARD_PORTCON_Init();
  BOARD_ADC_Init();

  CRC_Init();
  UART_Init();

  printf("hawk5\n");

  SYS_Main();
}
