#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../external/printf/printf.h"
#include "../inc/dp32g030/timer.h"

// =============================================================================
// НАСТРОЙКИ
// =============================================================================

#define TIMER_CLK_HZ 4000000UL     // 48МГц / 12 = 4 МГц
#define TIMER_TICKS_PER_US 4UL     // 1 мкс = 4 тика
#define TIMER_TICKS_PER_MS 4000UL  // 1 мс = 4000 тиков

// =============================================================================
// ГЛОБАЛЬНЫЕ
// =============================================================================

static volatile uint32_t timer0_overflow_cnt = 0;
static volatile uint32_t timer1_delay_remaining = 0;

// =============================================================================
// TIMER0 — 64-битный uptime (основной счётчик времени)
// =============================================================================

void TIMER0_InitAsUptimeCounter(void) {
  TIMERBASE0_EN = 0;
  TIMERBASE0_DIV = 12 - 1;      // 48 МГц / 12 = 4 МГц
  TIMERBASE0_LOW_LOAD = 0xFFFF; // Максимум для 16-бит счётчика
  TIMERBASE0_IF = 1;            // Сброс флага
  TIMERBASE0_IE = 1;            // Включить прерывание
  timer0_overflow_cnt = 0;
  NVIC_EnableIRQ(Interrupt5_IRQn);
  TIMERBASE0_EN = 1;
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

  // Общее количество тиков = (количество переполнений * 65536) + текущий счетчик
  uint64_t total_ticks = ((uint64_t)overflow1 << 16) + cnt;

  // Конвертируем в микросекунды (4 тика = 1 мкс)
  return total_ticks / TIMER_TICKS_PER_US;
}

uint64_t GetUptimeMs(void) { 
  return GetUptimeUs() / 1000; 
}

uint32_t GetUptimeSec(void) { 
  return (uint32_t)(GetUptimeUs() / 1000000); 
}

// =============================================================================
// TIMER1 — для задержек с прерыванием каждую 1 мс
// =============================================================================

void TIMER1_InitForDelay(void) {
  TIMERBASE1_EN = 0;
  TIMERBASE1_DIV = 48000 - 1;                 // 48 МГц / 48000 = 1 кГц
  TIMERBASE1_LOW_LOAD = 1 - 1;                // 1 тик = 1 мс при 1 кГц
  TIMERBASE1_IF = 1;                          // Сброс флага
  TIMERBASE1_IE = 1;                          // Включить прерывание
  timer1_delay_remaining = 0;
  NVIC_EnableIRQ(Interrupt6_IRQn);
  TIMERBASE1_EN = 1;
}

void HandlerTIMER_BASE1(void) {
  if (TIMERBASE1_IF & 1) {
    if (timer1_delay_remaining > 0) {
      timer1_delay_remaining--;
    }
    TIMERBASE1_IF = 1;
  }
}

void TIMER_DelayMs(uint32_t ms) {
  if (ms == 0) return;
  
  timer1_delay_remaining = ms;
  
  // Ждём завершения
  while (timer1_delay_remaining > 0) {
    __WFI();  // Энергосбережение - ждём прерывания
  }
}

// =============================================================================
// МИКРОСЕКУНДНЫЕ ЗАДЕРЖКИ (через TIMER0 uptime)
// =============================================================================

void TIMER_DelayUs(uint32_t us) {
  if (us == 0) return;

  uint64_t start = GetUptimeUs();
  uint64_t target = start + us;

  while (GetUptimeUs() < target) {
    // Активное ожидание для точности
  }
}

// =============================================================================
// ЗАДЕРЖКИ В ТИКАХ (250 нс, если 4 МГц)
// =============================================================================

void TIMER_DelayTicks(uint32_t ticks) {
  if (ticks == 0) return;

  // Используем TIMER1 для подсчёта тиков
  uint32_t start = TIMERBASE1_LOW_CNT;
  
  // Для больших задержек делаем несколько итераций
  while (ticks > 0) {
    uint32_t delay_chunk = (ticks > 4000) ? 4000 : ticks;
    uint32_t current = TIMERBASE1_LOW_CNT;
    uint32_t target = (current + delay_chunk) % TIMER_TICKS_PER_MS;
    
    // Ждём пока счётчик достигнет цели
    while (1) {
      current = TIMERBASE1_LOW_CNT;
      
      // Проверка с учётом переполнения
      if (target > (start % TIMER_TICKS_PER_MS)) {
        // Нет переполнения
        if (current >= target) break;
      } else {
        // Было переполнение
        if (current >= target && current < (start % TIMER_TICKS_PER_MS)) break;
      }
    }
    
    ticks -= delay_chunk;
    start = TIMERBASE1_LOW_CNT;
  }
}

void TIMER_Delay250ns(uint32_t count) { 
  TIMER_DelayTicks(count); 
}
