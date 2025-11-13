#ifndef FC_APP_H
#define FC_APP_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

void FC_init();
void FC_deinit();
void FC_update();
bool FC_key(KEY_Code_t key, Key_State_t state);
void FC_render();

#endif /* end of include guard: FC_APP_H */
