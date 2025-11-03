#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"

/* 48 MHz PCLK  →  DIV = 47  gives exactly 1 MHz tick (1 µs) */
#define TICKS_PER_US 1
#define TICKS_PER_MS 1000
#define TICKS_PER_SEC 1000000

void TIMER0_InitAsUptimeCounter(void) {
  TIMER0->DIV = 47;           /* 1 MHz tick */
  TIMER0->HIGH_LOAD = 0xFFFF; /* free-running */
  TIMER0->IF = 2;             /* clear stale */
  TIMER0->IE = 2;             /* enable HIGH overflow irq */
  NVIC_EnableIRQ(5);          /* TIMER0 irq number = 5 */
  TIMER0->EN = 0x00000002;    /* start HIGH counter */
}

static volatile uint32_t timer0_overflows = 0;

void HandlerTIMER_BASE0(void) /* вектор 5 в таблице DP32G030 */
{
  if (TIMER0->IF & 2) { /* HIGH counter overflow */
    TIMER0->IF = 2;     /* W1C */
    ++timer0_overflows; /* 65536 мкс каждый тик */
  }
}

uint64_t GetUptimeUs(void) {
  uint32_t cnt = TIMER0->HIGH_CNT;  /* 0-65535 */
  uint64_t high = timer0_overflows; /* 0,1,2… */
  return (high << 16) + cnt;        /* 1 LSB = 1 мкс */
}

uint64_t GetUptimeMs(void) { return GetUptimeUs() / 1000ULL; }
uint32_t GetUptimeSec(void) { return GetUptimeUs() / 1000000ULL; }

void TIMER1_InitForDelay(void) {
  TIMER1->EN = 0;   /* выключить оба счётчика */
  TIMER1->IE = 0;   /* выключить прерывания */
  TIMER1->IF = 3;   /* сбросить все флаги */
  TIMER1->DIV = 47; /* 1 MHz → 1 µs per tick */
}

static void timer1_wait_ticks(uint32_t ticks) {
  if (ticks == 0)
    return;
  if (ticks > 0x10000)
    ticks = 0x10000; /* max 65536 */

  TIMER1->EN = 0;               /* остановить */
  TIMER1->IF = 1;               /* сбросить флаг LOW */
  TIMER1->LOW_LOAD = ticks - 1; /* для N тиков нужно (N-1) */
  TIMER1->EN = 0x00000001;      /* запустить LOW counter (бит 0) */

  /* Ждать переполнения LOW (бит 0 в IF) */
  while (!(TIMER1->IF & 1))
    ;

  TIMER1->EN = 0; /* остановить */
  TIMER1->IF = 1; /* сбросить флаг */
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

/* void TIMER_DelayUs(uint32_t us) {
    TIMER_DelayTicks(us);
} */

void TIMER_DelayUs(uint32_t us) {
  if (us < 10) {
    /* Для очень коротких задержек (<10 µs) накладные расходы
       на таймер больше самой задержки - используем активное ожидание */
    uint64_t start = GetUptimeUs();
    while ((GetUptimeUs() - start) < us)
      ;
  } else {
    TIMER_DelayTicks(us);
  }
}

void TIMER_DelayMs(uint32_t ms) { TIMER_DelayUs(ms * 1000); }

/* 250 ns is 1/4 µs → 0.25 tick → round to 1 tick ≈ 250 ns @ 4 MHz
   (change DIV to 11 if you really need 4 MHz tick) */
void TIMER_Delay250ns(uint32_t count) {
  uint32_t old_div = TIMER1->DIV;
  TIMER1->DIV = 11; /* 48 MHz / 12 = 4 MHz */
  TIMER_DelayTicks(count);
  TIMER1->DIV = old_div;
}
