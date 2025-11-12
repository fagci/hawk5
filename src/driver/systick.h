#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/* ----------  hardware abstraction macros  ---------- */
#define TIMER0_BASE 0x40064000
#define TIMER1_BASE 0x40064800

typedef struct {
  volatile uint32_t EN;        // 0x00
  volatile uint32_t DIV;       // 0x04
  uint32_t _rsv[2];            // 0x08-0x0C (8 байт)
  volatile uint32_t IE;        // 0x10
  volatile uint32_t IF;        // 0x14
  uint32_t _rsv2[2];           // 0x18-0x1C (8 байт) ← ВОТ ОНО!
  volatile uint32_t HIGH_LOAD; // 0x20
  volatile uint32_t HIGH_CNT;  // 0x24
  uint32_t _rsv3[2];           // 0x28-0x2C (8 байт) ← И ЭТО!
  volatile uint32_t LOW_LOAD;  // 0x30
  volatile uint32_t LOW_CNT;   // 0x34
} TIMERBASE_TypeDef;

#define TIMER0 ((TIMERBASE_TypeDef *)TIMER0_BASE)
#define TIMER1 ((TIMERBASE_TypeDef *)TIMER1_BASE)

static void timer_base_enable_clk(void) {
  /* SYSCON->DEV_CLK_GATE bit 12 & 13 */
  *(volatile uint32_t *)0x40000008 |= (3UL << 12);
}

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

#endif
