#include "appslist.h"
#include "../helper/channels.h"
#include "../helper/menu.h"
#include "apps.h"
#include "chlist.h"
#include <sys/types.h>

static MenuItem appsItems[RUN_APPS_COUNT];

static Menu appsMenu = {"Apps", appsItems, RUN_APPS_COUNT};

static bool run(const MenuItem *item, KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && key == KEY_MENU) {
    APPS_exit();
    if (item->setting == APP_CH_LIST) {
      gChListFilter = TYPE_FILTER_CH;
    }
    APPS_runManual(item->setting);
    return true;
  }
  return false;
}

void APPSLIST_init(void) {
  for (uint8_t i = 0; i < RUN_APPS_COUNT; ++i) {
    AppType_t app = appsAvailableToRun[i];
    appsItems[i].name = apps[app].name;
    appsItems[i].action = run;
    appsItems[i].setting = app;
  }
  MENU_Init(&appsMenu);
}

bool APPSLIST_key(KEY_Code_t key, Key_State_t state) {
  if (MENU_HandleInput(key, state)) {
    return true;
  }
  return false;
}

void APPSLIST_render(void) { MENU_Render(); }
