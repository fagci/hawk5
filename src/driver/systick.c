#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"
#include <stdbool.h>

// 1 ms = 4000 ticks @ 4 MHz timer clock (0.25 us/tick)
static const uint32_t TICKS_PER_MS = 4000;
volatile uint32_t ms_since_boot = 0;
// Tick-based delay (hijacks timer; use sparingly)
volatile bool timer_expired = false;

void TIM0_INIT() {
  TIMERBASE0_EN = 0x00;               // Stop timer
  TIMERBASE0_DIV = 12;                // Divider for 4 MHz (48 MHz CPU / 12)
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS; // Load for 1 ms period
  TIMERBASE0_IF = 0x01;               // Clear interrupt flag
  TIMERBASE0_IE = 0x01;               // Enable interrupt
  NVIC_EnableIRQ(Interrupt5_IRQn);    // Enable NVIC

  ms_since_boot = 0;    // Reset counter
  TIMERBASE0_EN = 0x01; // Start timer in continuous mode
}

void HandlerTIMER_BASE0(void) {
  if (TIMERBASE0_IE & 0x01) { // Если прерывания включены = периодический режим
    ms_since_boot++;
    TIMERBASE0_IF = 0x01;
    TIMERBASE0_LOW_LOAD = TICKS_PER_MS;
    TIMERBASE0_EN = 0x01;
  } else {
    // В режиме polling для TIMER_DelayTicks - не должны сюда попадать
    TIMERBASE0_IF = 0x01;
  }
}

void TIMER_DelayTicks(uint32_t delay_in_ticks) {
  timer_expired = false;

  // Отключаем прерывание, чтобы не портить ms_since_boot
  TIMERBASE0_IE = 0x00; // ← ДОБАВИТЬ
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = delay_in_ticks;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_EN = 0x01;

  while (!timer_expired) {
    // Polling вместо __WFI(), т.к. прерывание отключено
    if (TIMERBASE0_IF & 0x01) {
      timer_expired = true;
      TIMERBASE0_IF = 0x01;
    }
  }

  // Восстанавливаем периодический режим
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_IE = 0x01; // ← Включаем обратно
  TIMERBASE0_EN = 0x01;
}

void TIMER_Delay250ns(const uint32_t Delay) { TIMER_DelayTicks(Delay); }

void TIMER_DelayUs(const uint32_t Delay) {
  if (Delay < 1000) {
    TIMER_DelayTicks(Delay * 4);
  } else {
    uint32_t ms = Delay / 1000;
    uint32_t us_remainder = Delay % 1000;
    TIMER_DelayMs(ms);
    if (us_remainder > 0) {
      TIMER_DelayTicks(us_remainder * 4);
    }
  }
}

// Recommended ms delay: busy-wait using global counter (no timer hijacking)
void TIMER_DelayMs(uint32_t delay_ms) {
  uint32_t start_ms = TIMER_GetMsSinceBoot();
  while ((uint32_t)(TIMER_GetMsSinceBoot() - start_ms) < delay_ms) {
    __WFI(); // Power-saving wait
  }
}

uint32_t TIMER_GetMsSinceBoot(void) {
  __disable_irq();
  uint32_t ms = ms_since_boot;
  __enable_irq();
  return ms;
}
