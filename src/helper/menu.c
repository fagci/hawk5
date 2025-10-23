#include "menu.h"
#include "../driver/uart.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "measurements.h"
#include "numnav.h"
#include <stdbool.h>
#include <string.h>

#define MENU_STACK_DEPTH 3

static Menu *menu_stack[MENU_STACK_DEPTH];
static uint8_t menu_stack_top = 0;

static Menu *active_menu = NULL;
static uint16_t current_index = 0;

static void (*renderFn)(uint8_t x, uint8_t y, const char *pattern, ...);

static void renderItem(uint16_t index, uint8_t i, bool isCurrent) {
  const MenuItem *item = &active_menu->items[index];
  const uint8_t ex = active_menu->x + active_menu->width;
  const uint8_t y = active_menu->y + i * active_menu->itemHeight;
  const uint8_t by = y + active_menu->itemHeight -
                     (active_menu->itemHeight >= MENU_ITEM_H ? 3 : 2);

  renderFn(3, by, "%s %c", item->name, item->submenu ? '>' : ' ');

  if (item->get_value_text) {
    char value_buf[32];
    item->get_value_text(item, value_buf, sizeof(value_buf));
    PrintSmallEx(ex - 7, by, POS_R, C_FILL, "%s", value_buf);
  }
}

static void init() {
  if (strlen(active_menu->title)) {
    STATUSLINE_SetText(active_menu->title);
  }

  if (active_menu->y < MENU_Y)
    active_menu->y = MENU_Y;

  if (!active_menu->width)
    active_menu->width = LCD_WIDTH;

  if (!active_menu->height)
    active_menu->height = LCD_HEIGHT - active_menu->y;

  if (!active_menu->itemHeight)
    active_menu->itemHeight = MENU_ITEM_H;

  if (active_menu->on_enter)
    active_menu->on_enter();

  if (!active_menu->render_item) {
    active_menu->render_item = renderItem;
  }

  renderFn = active_menu->itemHeight >= MENU_ITEM_H ? PrintMedium : PrintSmall;
}

void MENU_Init(Menu *main_menu) {
  Log("[MENU] Init");
  active_menu = main_menu;
  current_index = 0;
  menu_stack_top = 0;

  init();
}

void MENU_Deinit() { active_menu = NULL; }

void MENU_Render(void) {
  if (!active_menu)
    return;

  uint8_t itemsShow = active_menu->height / active_menu->itemHeight;

  const uint16_t offset = (current_index >= 2) ? current_index - 2 : 0;
  const uint16_t visible = MIN(active_menu->num_items, itemsShow);

  const uint8_t ex = active_menu->x + active_menu->width;
  const uint8_t ey = active_menu->y + active_menu->height;

  FillRect(active_menu->x, active_menu->y, active_menu->width,
           active_menu->height, C_CLEAR);

  for (uint16_t i = 0; i < visible; ++i) {
    uint16_t idx = i + offset;
    if (idx >= active_menu->num_items)
      break;

    const bool isActive = idx == current_index;
    const uint8_t y = active_menu->y + i * active_menu->itemHeight;

    active_menu->render_item(idx, i, isActive);

    if (isActive) {
      FillRect(active_menu->x, y, ex - 4, active_menu->itemHeight, C_INVERT);
    }
  }

  // scrollbar
  const uint8_t y = ConvertDomain(current_index, 0, active_menu->num_items - 1,
                                  active_menu->y, ey - 3);

  DrawVLine(ex - 2, active_menu->y, active_menu->height, C_FILL);

  FillRect(ex - 3, y, 3, 3, C_FILL);
}

static void setMenuIndex(uint16_t i) { current_index = i - 1; }

static bool handleNumNav(KEY_Code_t key, Key_State_t state) {
  if (gIsNumNavInput && key == KEY_EXIT) {
    NUMNAV_Deinit();
    return true;
  }
  if (!gIsNumNavInput &&
      ((state == KEY_LONG_PRESSED && key == KEY_STAR) ||
       (state == KEY_RELEASED && key >= KEY_0 && key <= KEY_9))) {
    NUMNAV_Init(current_index, 0, active_menu->num_items - 1);
    gNumNavCallback = setMenuIndex;
    return true;
  }
  if (state == KEY_RELEASED) {
    if (gIsNumNavInput) {
      current_index = NUMNAV_Input(key) - 1;
      return true;
    }
  }
  return false;
}

bool MENU_IsActive() { return active_menu; }

bool MENU_HandleInput(KEY_Code_t key, Key_State_t state) {
  if (!active_menu) {
    return false;
  }

  Log("[MENU] Key");

  if (!active_menu->items) {
    // Для меню без items (как chListMenu) используем упрощённую логику
    if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
      switch (key) {
      case KEY_UP:
      case KEY_DOWN:
        current_index =
            IncDecU(current_index, 0, active_menu->num_items, key == KEY_DOWN);
        return true;
      default:
        break;
      }
    }
    if (active_menu->action && active_menu->action(current_index, key, state)) {
      return true;
    }
    if (handleNumNav(key, state)) {
      return true;
    }
    // Меню без items не обрабатывает другие действия
    return false;
  }

  const MenuItem *item = &active_menu->items[current_index];

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      current_index =
          IncDecU(current_index, 0, active_menu->num_items, key == KEY_DOWN);
      return true;
      break;
    case KEY_STAR:
    case KEY_F:
      if (item->change_value) {
        item->change_value(item, key == KEY_STAR);
        return true;
      }

      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_MENU:
      if (item->submenu) {
        if (menu_stack_top < MENU_STACK_DEPTH) {
          menu_stack[menu_stack_top++] = active_menu;
          active_menu = item->submenu;
          current_index = 0;
          init();
        }
        return true;
      }
      break;
    case KEY_EXIT:
      return MENU_Back();
    default:
      break;
    }
  }
  if (item->action && item->action(item, key, state)) {
    return true;
  }
  if (handleNumNav(key, state)) {
    return true;
  }
  return false;
}

bool MENU_Back() {
  if (menu_stack_top > 0) {
    active_menu = menu_stack[--menu_stack_top];
    current_index = 0;
    init();

    return true;
  }
  active_menu = NULL;
  return false;
}
