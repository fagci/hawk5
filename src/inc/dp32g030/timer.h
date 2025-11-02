//
// Created by RUPC on 2024/1/8.
//

#ifndef HARDWARE_DP32G030_TIMER_H
#define HARDWARE_DP32G030_TIMER_H
#include <stdint.h>

#define TIMERBASE0_ADD 0x40064000
#define TIMERBASE1_ADD 0x40064400

// === TIMER0 Registers ===
#define TIMERBASE0_EN_ADD         (0x00 + TIMERBASE0_ADD)
#define TIMERBASE0_DIV_ADD        (0x04 + TIMERBASE0_ADD)
#define TIMERBASE0_IE_ADD         (0x10 + TIMERBASE0_ADD)
#define TIMERBASE0_IF_ADD         (0x14 + TIMERBASE0_ADD)
#define TIMERBASE0_HIGH_LOAD_ADD  (0x20 + TIMERBASE0_ADD)
#define TIMERBASE0_HIGH_CNT_ADD   (0x24 + TIMERBASE0_ADD)
#define TIMERBASE0_LOW_LOAD_ADD   (0x30 + TIMERBASE0_ADD)
#define TIMERBASE0_LOW_CNT_ADD    (0x34 + TIMERBASE0_ADD)

// === TIMER1 Registers ===
#define TIMERBASE1_EN_ADD         (0x00 + TIMERBASE1_ADD)
#define TIMERBASE1_DIV_ADD        (0x04 + TIMERBASE1_ADD)
#define TIMERBASE1_IE_ADD         (0x10 + TIMERBASE1_ADD)
#define TIMERBASE1_IF_ADD         (0x14 + TIMERBASE1_ADD)
#define TIMERBASE1_HIGH_LOAD_ADD  (0x20 + TIMERBASE1_ADD)
#define TIMERBASE1_HIGH_CNT_ADD   (0x24 + TIMERBASE1_ADD)
#define TIMERBASE1_LOW_LOAD_ADD   (0x30 + TIMERBASE1_ADD)
#define TIMERBASE1_LOW_CNT_ADD    (0x34 + TIMERBASE1_ADD)

// === TIMER0 Macros ===
#define TIMERBASE0_EN        (*(volatile uint32_t *)TIMERBASE0_EN_ADD)
#define TIMERBASE0_DIV       (*(volatile uint32_t *)TIMERBASE0_DIV_ADD)
#define TIMERBASE0_IE        (*(volatile uint32_t *)TIMERBASE0_IE_ADD)
#define TIMERBASE0_IF        (*(volatile uint32_t *)TIMERBASE0_IF_ADD)
#define TIMERBASE0_HIGH_LOAD (*(volatile uint32_t *)TIMERBASE0_HIGH_LOAD_ADD)
#define TIMERBASE0_HIGH_CNT  (*(volatile uint32_t *)TIMERBASE0_HIGH_CNT_ADD)
#define TIMERBASE0_LOW_LOAD  (*(volatile uint32_t *)TIMERBASE0_LOW_LOAD_ADD)
#define TIMERBASE0_LOW_CNT   (*(volatile uint32_t *)TIMERBASE0_LOW_CNT_ADD)

// === TIMER1 Macros ===
#define TIMERBASE1_EN        (*(volatile uint32_t *)TIMERBASE1_EN_ADD)
#define TIMERBASE1_DIV       (*(volatile uint32_t *)TIMERBASE1_DIV_ADD)
#define TIMERBASE1_IE        (*(volatile uint32_t *)TIMERBASE1_IE_ADD)
#define TIMERBASE1_IF        (*(volatile uint32_t *)TIMERBASE1_IF_ADD)
#define TIMERBASE1_HIGH_LOAD (*(volatile uint32_t *)TIMERBASE1_HIGH_LOAD_ADD)
#define TIMERBASE1_HIGH_CNT  (*(volatile uint32_t *)TIMERBASE1_HIGH_CNT_ADD)
#define TIMERBASE1_LOW_LOAD  (*(volatile uint32_t *)TIMERBASE1_LOW_LOAD_ADD)
#define TIMERBASE1_LOW_CNT   (*(volatile uint32_t *)TIMERBASE1_LOW_CNT_ADD)


extern volatile uint32_t TIM0_CNT;

void TIM0_INIT();

#endif //UV_K5_FIRMWARE_CUSTOM_0_17_TIMER_H
