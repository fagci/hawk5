#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

// Инициализация
void TIMER0_InitAsUptimeCounter(void); // Основной uptime
void TIMER1_InitForDelay(void);        // Только задержки

// Задержки
void TIMER_DelayMs(uint32_t ms);
void TIMER_DelayUs(uint32_t us);
void TIMER_DelayTicks(uint32_t ticks);
void TIMER_Delay250ns(uint32_t count);

// Uptime (время с момента включения)
uint64_t GetUptimeUs(void);  // микросекунды
uint64_t GetUptimeMs(void);  // миллисекунды
uint32_t GetUptimeSec(void); // секунды

// Обработчик (из start.S)
void HandlerTIMER_BASE0(void);

#endif
