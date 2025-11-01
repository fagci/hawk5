#include "scheduler.h"
#include "driver/systick.h"
#include "inc/dp32g030/timer.h"

static uint32_t elapsedMilliseconds = 0;

uint32_t Now(void) { return TIM0_CNT / 4; }

void SetTimeout(uint32_t *v, uint32_t t) {
  *v = t == UINT32_MAX ? UINT32_MAX : Now() + t;
}

bool CheckTimeout(uint32_t *v) { return Now() >= *v; }

