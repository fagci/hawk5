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

static const char *APP_NAME = "hawk5";
#ifdef __cplusplus
extern "C" {
#endif

__attribute__((externally_visible, used)) void
HardFault_Handler_C(uint32_t *hardfault_args) {
  volatile uint32_t stacked_r0 = hardfault_args[0];
  volatile uint32_t stacked_r1 = hardfault_args[1];
  volatile uint32_t stacked_r2 = hardfault_args[2];
  volatile uint32_t stacked_r3 = hardfault_args[3];
  volatile uint32_t stacked_r12 = hardfault_args[4];
  volatile uint32_t stacked_lr = hardfault_args[5];
  volatile uint32_t stacked_pc = hardfault_args[6];
  volatile uint32_t stacked_psr = hardfault_args[7];

  printf("\n[HARDFAULT]\n");
  printf("R0:  0x%08lX\n", stacked_r0);
  printf("R1:  0x%08lX\n", stacked_r1);
  printf("R2:  0x%08lX\n", stacked_r2);
  printf("R3:  0x%08lX\n", stacked_r3);
  printf("R12: 0x%08lX\n", stacked_r12);
  printf("LR:  0x%08lX\n", stacked_lr);
  printf("PC:  0x%08lX <- see .map (0=NULLptr)\n", stacked_pc);
  printf("PSR: 0x%08lX\n", stacked_psr);

  while (1)
    ; // Зависаем
}

// Naked функция для Cortex-M0 (БЕЗ ite)
__attribute__((naked)) void HandlerHardFault(void) {
  __asm volatile("movs   r0, #4          \n"
                 "mov    r1, lr          \n"
                 "tst    r0, r1          \n"
                 "beq    _MSP            \n"
                 "mrs    r0, psp         \n"
                 "b      _HALT           \n"
                 "_MSP:                      \n"
                 "mrs    r0, msp         \n"
                 "_HALT:                     \n"
                 "ldr r1,=HardFault_Handler_C \n"
                 "bx r1                  \n"
                 "bkpt #0                \n");
}

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
  /* EEPROM_TestReadSpeedFast();
  for (;;) {
  } */
  /* EEPROM_ScanBus();
  for (;;) {
  } */

  printf("hawk5\n");

  SYS_Main();
}
#ifdef __cplusplus
}
#endif
