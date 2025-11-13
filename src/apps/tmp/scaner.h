#ifndef SCANER_H
#define SCANER_H

#include "../driver/keyboard.h"
#include "../radio.h"
#include <stdbool.h>
#include <stdint.h>

bool SCANER_key(KEY_Code_t Key, Key_State_t state);
void SCANER_init(void);
void SCANER_deinit(void);
void SCANER_update(void);
void SCANER_render(void);

#endif /* end of include guard: SCANER_H */
