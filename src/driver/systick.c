#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"
#include <stdbool.h>

static const uint32_t TICKS_PER_MS = 4000;  // 1 ms = 4000 ticks @ 4 MHz timer clock (0.25 us/tick)
volatile uint32_t ms_since_boot = 0;  // Global millisecond counter
// Tick-based delay (hijacks timer; use sparingly)
volatile bool timer_expired = false;


void TIM0_INIT() {
  TIMERBASE0_EN = 0x00;  // Stop timer
  TIMERBASE0_DIV = 12;   // Divider for 4 MHz (48 MHz CPU / 12)
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;  // Load for 1 ms period
  TIMERBASE0_IF = 0x01;  // Clear interrupt flag
  TIMERBASE0_IE = 0x01;  // Enable interrupt
  NVIC_EnableIRQ(Interrupt5_IRQn);  // Enable NVIC

  ms_since_boot = 0;     // Reset counter
  TIMERBASE0_EN = 0x01;  // Start timer in continuous mode
}

void HandlerTIMER_BASE0(void) {
  ms_since_boot++;       // Increment ms counter
  TIMERBASE0_IF = 0x01;  // Clear flag
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;  // Explicit reload for periodic (ensures continuation if no auto-reload)
  TIMERBASE0_EN = 0x01;  // Re-enable if timer stops on underflow
  timer_expired = true;  // For one-shot delays
}

// Get milliseconds since boot
uint32_t TIMER_GetMsSinceBoot(void) {
  return ms_since_boot;
}

// Recommended ms delay: busy-wait using global counter (no timer hijacking)
void TIMER_DelayMs(uint32_t delay_ms) {
  uint32_t start_ms = TIMER_GetMsSinceBoot();
  while (TIMER_GetMsSinceBoot() - start_ms < delay_ms) {
    __WFI();  // Power-saving wait
  }
}

void TIMER_DelayTicks(uint32_t delay_in_ticks) {
  timer_expired = false;

  // Pause periodic mode
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = delay_in_ticks;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_IE = 0x01;
  TIMERBASE0_EN = 0x01;  // Start one-shot

  while (!timer_expired) {
    __WFI();
  }

  // Restore periodic mode
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_EN = 0x01;
}

// Delay wrappers (update to use ms-based for better compatibility)
void TIMER_DelayUs(const uint32_t Delay) { 
  // TIMER_DelayTicks(Delay * 4);  // Old tick-based
  TIMER_DelayMs(Delay / 1000);  // Switch to ms-based for simplicity; adjust if sub-ms needed
}
void TIMER_Delay250ns(const uint32_t Delay) { TIMER_DelayTicks(Delay); }
