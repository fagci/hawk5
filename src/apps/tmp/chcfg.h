#ifndef CHCFG_H
#define CHCFG_H

#include "../driver/keyboard.h"
#include "../helper/channels.h"
#include <stdbool.h>
#include <stdint.h>

void CHCFG_init();
void CHCFG_deinit();
bool CHCFG_key(KEY_Code_t key, Key_State_t state);
void CHCFG_render();

extern CH gChEd;
extern int16_t gChNum;

#endif /* end of include guard: CHCFG_H */
