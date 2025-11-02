#include "board.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "external/printf/printf.h"
#include "inc/dp32g030/pmu.h"
#include "inc/dp32g030/syscon.h"
#include "inc/dp32g030/timer.h"
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

  BOARD_GPIO_Init();
  BOARD_PORTCON_Init();
  BOARD_ADC_Init();

  CRC_Init();
  UART_Init();
  TIMER1_InitForDelay();
  TIMER0_InitAsUptimeCounter();
  /* while (1) {
    printf("Tick\n");
    TIMER_DelayMs(1000); // ← 1 секунда
  } */

  /* TIMERBASE1_EN = 0;
  TIMERBASE1_DIV = 12;
  TIMERBASE1_HIGH_LOAD = 0xFFFF;
  TIMERBASE1_LOW_LOAD = 0xFFFFFFFF;
  TIMERBASE1_IE = 0;
  TIMERBASE1_EN = 1;
  printf("SYSCON_DEV_CLK_GATE = 0x%08lX\n", SYSCON_DEV_CLK_GATE);

  printf("DIV=%lu\n", TIMERBASE1_DIV); */










  /* while (1) {
    uint64_t start = GetUptimeUs();
    printf("Tick\n");
    TIMER_DelayMs(1000);
    uint64_t end = GetUptimeUs();
    printf("Real delay: %lu us (%lu)\n", (uint32_t)(end - start),
           GetUptimeMs());
  } */


  printf("hawk5\n");

  SYS_Main();
}
