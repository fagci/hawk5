#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void TIM0_INIT(void);
void HandlerTIMER_BASE0(void);

void TIM1_INIT(void);          // Новая
void HandlerTIMER_BASE1(void); // Новая

uint32_t TIMER_GetMsSinceBoot(void);  // Через TIMER0
uint32_t TIMER1_GetMsSinceBoot(void); // Через TIMER1 (новая)

void TIMER_DelayMs(uint32_t ms);
void TIMER_DelayTicks(uint32_t ticks);
void TIMER_DelayUs(uint32_t us);
void TIMER_Delay250ns(uint32_t count);

#endif
