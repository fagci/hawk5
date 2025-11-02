#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../external/printf/printf.h"
#include "../inc/dp32g030/timer.h"

// =============================================================================
// НАСТРОЙКИ
// =============================================================================

#define TIMER0_CLK_HZ 4000000UL // 48МГц / 12 = 4 МГц
#define TIMER0_TICKS_PER_US 4UL // 1 мкс = 4 тика

// =============================================================================
// ГЛОБАЛЬНЫЕ
// =============================================================================

static volatile uint32_t timer0_overflow_cnt = 0;

// =============================================================================
// TIMER0 — 64-битный uptime (основной счётчик времени)
// =============================================================================



void TIMER0_InitAsUptimeCounter(void) {
  TIMERBASE0_EN = 0;
  TIMERBASE0_DIV = 12;          // 4 МГц
  TIMERBASE0_LOW_LOAD = 0xFFFF; // Максимум ~65536 для LOW
  TIMERBASE0_IE = 1; // Прерывание при переполнении LOW
  TIMERBASE0_IF = 1;
  NVIC_EnableIRQ(5);
  timer0_overflow_cnt = 0;
  TIMERBASE0_EN = 1; // Старт
}

void HandlerTIMER_BASE0(void) {
  if (TIMERBASE0_IF & 1) {
    timer0_overflow_cnt++;
    TIMERBASE0_IF = 1;
  }
}

uint64_t GetUptimeUs(void) {
  uint32_t cnt, overflow1, overflow2;

  // Безопасное чтение с учетом возможного прерывания
  do {
    overflow1 = timer0_overflow_cnt;
    cnt = TIMERBASE0_LOW_CNT;
    overflow2 = timer0_overflow_cnt;
  } while (overflow1 != overflow2);

  // Общее количество тиков = (количество переполнений * 65536) + текущий
  // счетчик
  uint64_t total_ticks = ((uint64_t)overflow1 * 65536ULL) + cnt;

  // Конвертируем в микросекунды (4 тика = 1 мкс)
  return total_ticks / TIMER0_TICKS_PER_US;
}

// =============================================================================
// TIMER1 — только для задержек (не использует IRQ)
// =============================================================================

void TIMER1_InitForDelay(void) {
  TIMERBASE1_EN = 0;
  TIMERBASE1_DIV = 12;          // 4 МГц
  TIMERBASE1_LOW_LOAD = 0xFFFF; // Максимум для LOW таймера
  TIMERBASE1_IE = 0;
  TIMERBASE1_EN = 1;
}

void TIMER_DelayTicks(uint32_t ticks) {
  if (ticks == 0)
    return;

  // Для больших задержек делаем несколько итераций
  while (ticks > 0) {
    uint32_t delay_chunk = (ticks > 60000) ? 60000 : ticks;

    uint32_t start = TIMERBASE1_LOW_CNT;
    uint32_t target = start + delay_chunk;

    // Обработка переполнения LOW (~65536)
    if (target >= 65536) {
      target -= 65536;
      // Ждем переполнения
      while (TIMERBASE1_LOW_CNT >= start)
        ;
      // Теперь ждем target после переполнения
      while (TIMERBASE1_LOW_CNT < target)
        ;
    } else {
      // Нет переполнения
      while (TIMERBASE1_LOW_CNT < target)
        ;
    }

    ticks -= delay_chunk;
  }
}

void TIMER_DelayUs(uint32_t us) {
  if (us == 0)
    return;
  TIMER_DelayTicks(us * TIMER0_TICKS_PER_US);
}
void TIMER_DelayMs(uint32_t ms) {
  uint32_t ticks = ms * 1000 * TIMER0_TICKS_PER_US;
  printf("Delay %lu ms = %lu ticks\n", ms, ticks);
  TIMER_DelayTicks(ticks);
}

/* void TIMER_DelayMs(uint32_t ms) {
  while (ms >= 1000) {
    TIMER_DelayUs(1000000);
    ms -= 1000;
  }
  if (ms > 0) {
    TIMER_DelayUs(ms * 1000);
  }
} */

void TIMER_Delay250ns(uint32_t count) { TIMER_DelayTicks(count); }

// =============================================================================
// ФУНКЦИИ ВРЕМЕНИ (через TIMER0)
// =============================================================================

uint64_t GetUptimeMs(void) { return GetUptimeUs() / 1000; }

uint32_t GetUptimeSec(void) { return (uint32_t)(GetUptimeUs() / 1000000); }
