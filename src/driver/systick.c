#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"
#include <stdbool.h>

static const uint32_t TICK_MULTIPLIER = 48;

volatile bool delay_complete = false;

void TIM0_INIT() {
  // Пример инициализации. Настроить делитель и отключить таймер перед
  // использованием
  TIMERBASE0_EN = 0x00;
  // Настроить делитель под 1 мкс тик (предположим CPU 48MHz)
  TIMERBASE0_DIV = 12;
  // Сбросить флаг прерывания
  TIMERBASE0_IF = 0x01;
  // Отключаем прерывания
  TIMERBASE0_IE = 0x00;
  NVIC_DisableIRQ(Interrupt5_IRQn);
}

volatile uint32_t TIM0_CNT = 0; // Счетчик прерываний
volatile bool timer_expired = false;

void HandlerTIMER_BASE0(void) {
  // Можно использовать для подсчёта тиков
  TIM0_CNT++;
  // Сбросить флаг прерывания (важно для повторной работы таймера)
  TIMERBASE0_IF = 0x01;
  timer_expired = true;
}

void TIMER_DelayTicks(uint32_t delay_in_ticks) {
  timer_expired = false;
  TIMERBASE0_LOW_LOAD = delay_in_ticks;
  TIMERBASE0_IF = 0x01; // Сбросить флаг
  TIMERBASE0_IE = 0x01; // Включить прерывание
  NVIC_EnableIRQ(Interrupt5_IRQn);

  TIMERBASE0_EN = 0x01; // Запустить таймер

  while (!timer_expired) {
    __WFI(); // Ожидание прерывания (экономия)
  }

  TIMERBASE0_EN = 0x00; // Остановить таймер
  TIMERBASE0_IE = 0x00; // Отключить прерывания
  NVIC_DisableIRQ(Interrupt5_IRQn);
}

void TIMER_DelayUs(const uint32_t Delay) { TIMER_DelayTicks(Delay * 4); }

void TIMER_Delay250ns(const uint32_t Delay) { TIMER_DelayTicks(Delay); }
