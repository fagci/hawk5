#ifndef RESET_H
#define RESET_H

#include "../driver/keyboard.h"

void RESET_Init();
void RESET_Update();
void RESET_Render();
bool RESET_key(KEY_Code_t k, Key_State_t st);

#endif /* end of include guard: RESET_H */
