#ifndef CHSCAN_H
#define CHSCAN_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

bool CHSCAN_key(KEY_Code_t Key, Key_State_t state);
void CHSCAN_init(void);
void CHSCAN_deinit(void);
void CHSCAN_update(void);
void CHSCAN_render(void);

#endif /* end of include guard: CHSCAN_H */
