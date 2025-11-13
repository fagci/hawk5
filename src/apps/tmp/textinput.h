#ifndef TEXTINPUT_H
#define TEXTINPUT_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

void TEXTINPUT_init();
bool TEXTINPUT_key(KEY_Code_t key, Key_State_t state);
void TEXTINPUT_update();
void TEXTINPUT_render();
void TEXTINPUT_deinit();

extern char *gTextinputText;
extern uint8_t gTextInputSize;
extern void (*gTextInputCallback)(void);

#endif /* end of include guard: TEXTINPUT_H */
