#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"

// Частота ядра: 48 МГц
// Делитель: 12 → тактовая частота таймера: 48МГц / 12 = 4 МГц
// 1 мс = 4000 тиков
static const uint32_t TICKS_PER_MS = 4000;

// Глобальные счётчики (объявлены в timer.h)
volatile uint32_t TIM0_CNT = 0;
volatile uint32_t TIM1_CNT = 0;

// === TIMER0: используется для задержек (как в оригинале) ===
void TIM0_INIT(void) {
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_DIV = 12;
  TIMERBASE0_LOW_LOAD = TICKS_PER_MS;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_IE = 0x01;
  NVIC_EnableIRQ(Interrupt5_IRQn);  // Предполагается, что TIMER0 → IRQ5
  TIM0_CNT = 0;
  TIMERBASE0_EN = 0x01;
}

void HandlerTIMER_BASE0(void) {
  TIM0_CNT++;
  TIMERBASE0_IF = 0x01;
}

// === TIMER1: независимый счётчик времени с момента запуска ===
void TIM1_INIT(void) {
  TIMERBASE1_EN = 0x00;
  TIMERBASE1_DIV = 12;                    // Та же частота: 4 МГц
  TIMERBASE1_LOW_LOAD = TICKS_PER_MS;     // 1 мс
  TIMERBASE1_IF = 0x01;
  TIMERBASE1_IE = 0x01;
  NVIC_EnableIRQ(Interrupt6_IRQn);        // Предполагается IRQ6 для TIMER1
  TIM1_CNT = 0;
  TIMERBASE1_EN = 0x01;
}

void HandlerTIMER_BASE1(void) {
  TIM1_CNT++;
  TIMERBASE1_IF = 0x01;
}

// === Функции для получения времени ===
uint32_t TIMER_GetMsSinceBoot(void) {
  return TIM0_CNT;  // Совместимость с текущим кодом
}

uint32_t TIMER1_GetMsSinceBoot(void) {
  return TIM1_CNT;  // Новый независимый счётчик
}

// === Задержки через TIMER0 (оставляем без изменений) ===
void TIMER_DelayMs(uint32_t ms) {
  uint32_t start = TIM0_CNT;
  while ((TIM0_CNT - start) < ms) {
    __WFI();
  }
}

void TIMER_DelayTicks(uint32_t ticks) {
  TIMERBASE0_IE = 0x00;
  TIMERBASE0_EN = 0x00;
  TIMERBASE0_LOW_LOAD = ticks;
  TIMERBASE0_IF = 0x01;
  TIMERBASE0_EN = 0x01;

  while (!(TIMERBASE0_IF & 0x01))
    ;

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
    TIMER_DelayTicks(us * 4);  // 4 МГц → 1 мкс = 4 тика
  }
}

void TIMER_Delay250ns(uint32_t count) {
  TIMER_DelayTicks(count);  // 4 МГц → 1 тик = 250 нс
}
