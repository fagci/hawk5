#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../inc/dp32g030/timer.h"
#include <stdbool.h>

static const uint32_t TICK_MULTIPLIER = 48;

void SYSTICK_Init(void) { SysTick_Config(48000); }

/* void SYSTICK_DelayTicks(const uint32_t ticks) {
  uint32_t elapsed_ticks = 0;
  uint32_t Start = SysTick->LOAD;
  uint32_t Previous = SysTick->VAL;

  do {
    uint32_t Current = SysTick->VAL;
    if (Current != Previous) {
      uint32_t Delta = (Current < Previous) ? (Previous - Current)
                                            : (Start - Current + Previous);

      elapsed_ticks += Delta;
      Previous = Current;
    }
  } while (elapsed_ticks < ticks);
} */

void SYSTICK_DelayTicks(const uint32_t ticks) {
  uint32_t elapsed_ticks = 0;
  uint32_t Start = SysTick->LOAD;
  uint32_t Previous = SysTick->VAL;

  do {
    uint32_t Current = SysTick->VAL;
    if (Current != Previous) {
      uint32_t Delta;
      if (Current < Previous) {
        Delta = Previous - Current;
      } else {
        // Произошло переполнение
        Delta = (Start + 1) - Current + Previous;
      }

      elapsed_ticks += Delta;
      Previous = Current;
    }
  } while (elapsed_ticks < ticks);
}

void SYSTICK_DelayUs(const uint32_t Delay) {
  SYSTICK_DelayTicks(Delay * TICK_MULTIPLIER);
}

void SYSTICK_Delay250ns(const uint32_t Delay) {
  SYSTICK_DelayTicks(Delay * TICK_MULTIPLIER / 4);
}

volatile bool delay_complete = false;

void TIM0_INIT() {
  TIMERBASE0_DIV = 4800; // Делитель тактовой частоты таймера
  TIMERBASE0_LOW_LOAD =
      10000; // Количество тактов для задержки (пример 1 секунда)

  TIMERBASE0_IF = 0x01; // Сбросить флаг прерывания
  TIMERBASE0_IE = 0x01; // Включить прерывания таймера
  TIMERBASE0_EN = 0x01; // Запустить таймер
  NVIC_SetPriority(Interrupt5_IRQn,
                   0); // Приоритет прерывания (Interrupt5_IRQn — это номер
                       // прерывания для TIMER_BASE0)
  NVIC_EnableIRQ(Interrupt5_IRQn); // Включить прерывания в NVIC
}

volatile uint32_t TIM0_CNT = 0; // Счетчик прерываний

void HandlerTIMER_BASE0(void) {
  TIM0_CNT++; // Можно использовать для подсчёта тиков
  TIMERBASE0_IF |=
      0x01; // Сбросить флаг прерывания (важно для повторной работы таймера)
}

void TIMER_DelayTicks(uint32_t delay_in_ticks) {
  TIM0_CNT = 0;
  TIMERBASE0_LOW_LOAD = delay_in_ticks;
  TIMERBASE0_EN |= 0x01; // Запустить таймер

  while (TIM0_CNT == 0) {
    __WFI(); // Ожидание прерывания (экономия энергии)
  }

  TIMERBASE0_EN &= ~0x01; // Остановить таймер
}

void TIMER_DelayUs(const uint32_t Delay) {
  TIMER_DelayTicks(Delay * TICK_MULTIPLIER);
}

void TIMER_Delay250ns(const uint32_t Delay) {
  TIMER_DelayTicks(Delay * TICK_MULTIPLIER / 4);
}
