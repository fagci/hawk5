#ifndef MENU_H

#define MENU_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

#define MENU_Y 8
#define MENU_ITEM_H 11
#define MENU_LINES_TO_SHOW 5

typedef void (*MenuAction)(void);
typedef void (*MenuOnEnter)(void);
typedef void (*MenuRenderItem)(uint16_t index, uint8_t visIndex,
                               bool is_selected);

typedef struct MenuItem MenuItem;

typedef struct MenuItem {
  const char *name;
  uint8_t setting; // настройка, которую меняем
  void (*get_value_text)(const MenuItem *item, char *buf, uint8_t buf_size);
  void (*change_value)(const MenuItem *item, bool up);
  struct Menu *submenu; // если не NULL — переход в подменю

  /* int64_t (*get_value)(const MenuItem *item);
  void (*set_value)(const MenuItem *item, int64_t); */

  bool (*action)(const MenuItem *item, KEY_Code_t, Key_State_t);
  // const void *user_data; // Данные для action
} MenuItem;

typedef struct Menu {
  const char *title;
  const MenuItem *items;
  uint16_t num_items;
  MenuRenderItem render_item;
  MenuOnEnter on_enter;
  uint8_t itemHeight;
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
} Menu;

void MENU_Init(Menu *main_menu);
void MENU_Render(void);
bool MENU_HandleInput(KEY_Code_t key, Key_State_t state);
bool MENU_Back(void);

#endif /* end of include guard: MENU_H */
