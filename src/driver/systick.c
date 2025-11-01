#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"

static const uint32_t TICKS_PER_MS = 4000;
static volatile uint32_t ms_counter = 0;

void TIM0_INIT(void) {
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_DIV = 12;
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_IE = 0x01;
  NVIC_EnableIRQ(Interrupt5_IRQn);
  ms_counter = 0;
  TIMERBASE0_EN = 0x01;
}

void HandlerTIMER_BASE0(void) {
  ms_counter++;
  TIMERBASE0_IF = 0x01;
}

uint32_t TIMER_GetMsSinceBoot(void) {
  return ms_counter;
}

void TIMER_DelayMs(uint32_t ms) {
  uint32_t start = ms_counter;
  while ((ms_counter - start) < ms) {
    __WFI();
  }
}

static void DelayTicks(uint32_t ticks) {
  TIMERBASE0_IE = 0x00;
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = ticks;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_EN = 0x01;
  
  while (!(TIMERBASE0_IF & 0x01));
  
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_IE = 0x01;
  TIMERBASE0_EN = 0x01;
}

void TIMER_DelayUs(uint32_t us) {
  if (us >= 1000) {
    TIMER_DelayMs(us / 1000);
    us %= 1000;
  }
  
  if (us > 0) {
    DelayTicks(us * 4);
  }
}

void TIMER_Delay250ns(uint32_t delay) {
  DelayTicks(delay);
}
