#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"
/* 48 MHz PCLK  →  DIV = 47  gives exactly 1 MHz tick (1 µs) */
#define TICKS_PER_US 1
#define TICKS_PER_MS 1000
#define TICKS_PER_SEC 1000000
void TIMER0_InitAsUptimeCounter(void) {
  TIMER0->DIV = 47;
  TIMER0->HIGH_LOAD = 0xFFFF;
  TIMER0->IF = 2;
  TIMER0->IE = 2;
  NVIC_EnableIRQ(5);
  TIMER0->EN = 0x00000002;
}
static volatile uint32_t uptime_ms = 0;
static volatile uint16_t accum_us = 0;
void HandlerTIMER_BASE0(void) {
  if (TIMER0->IF & 2) {
    TIMER0->IF = 2;
    uptime_ms += 65;
    accum_us += 536;
    if (accum_us >= 1000) {
      uptime_ms += 1;
      accum_us -= 1000;
    }
  }
}
uint32_t GetUptimeMs(void) {
  uint32_t ms1, ms2;
  uint16_t acc1, acc2;
  uint32_t cnt;
  do {
    ms1 = uptime_ms;
    acc1 = accum_us;
    cnt = TIMER0->HIGH_CNT;
    ms2 = uptime_ms;
    acc2 = accum_us;
  } while (ms1 != ms2 || acc1 != acc2);
  uint32_t sub = acc1 + cnt;
  return ms1 + sub / 1000;
}
uint32_t GetUptimeSec(void) { return GetUptimeMs() / 1000; }
void TIMER1_InitForDelay(void) {
  TIMER1->EN = 0;
  TIMER1->IE = 0;
  TIMER1->IF = 3;
  TIMER1->DIV = 47;
}
static void timer1_wait_ticks(uint32_t ticks) {
  if (ticks == 0)
    return;
  if (ticks > 0x10000)
    ticks = 0x10000; /* max 65536 */
  TIMER1->EN = 0;
  TIMER1->IF = 1;
  TIMER1->LOW_LOAD = ticks - 1;
  TIMER1->EN = 0x00000001;
  /* Ждать переполнения LOW (бит 0 в IF) */
  while (!(TIMER1->IF & 1))
    ;
  TIMER1->EN = 0;
  TIMER1->IF = 1;
}
void TIMER_DelayTicks(uint32_t ticks) {
  /* Разбиваем на куски по 65536 тиков */
  while (ticks > 0x10000) {
    timer1_wait_ticks(0x10000);
    ticks -= 0x10000;
  }
  if (ticks > 0) {
    timer1_wait_ticks(ticks);
  }
}
void TIMER_DelayUs(uint32_t us) { TIMER_DelayTicks(us); }
void TIMER_DelayMs(uint32_t ms) { TIMER_DelayUs(ms * 1000); }
