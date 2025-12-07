#include "regs-menu.h"
#include "../apps/apps.h"
#include "../external/printf/printf.h"
#include "../helper/menu.h"
#include "../radio.h"
#include "channels.h"

static bool inMenu;

static MenuItem menuItems[11];

static Menu regsMenu = {
    .title = "",
    .items = menuItems,
    .itemHeight = 7,
    .width = 64,
};

static void initMenu();

static void getValS(const MenuItem *item, char *buf, uint8_t buf_size) {
  VFOContext *ctx = &RADIO_GetCurrentVFO(gRadioState)->context;
  sprintf(buf, "%s", RADIO_GetParamValueString(ctx, item->setting));
}

static void updateVal(const MenuItem *item, bool up) {
  VFOContext *ctx = &RADIO_GetCurrentVFO(gRadioState)->context;
  RADIO_IncDecParam(ctx, item->setting, up, true);
  if (item->setting == PARAM_RADIO) {
    initMenu();
  }
}

static const ParamType paramsBK4819[] = {
    PARAM_RADIO,         //
    PARAM_GAIN,          //
    PARAM_BANDWIDTH,     //
    PARAM_SQUELCH_VALUE, //
    PARAM_MODULATION,    //
    PARAM_STEP,          //
    PARAM_AFC,           //
    PARAM_DEV,           //
    PARAM_XTAL,          //
    PARAM_FILTER,        //
    PARAM_POWER,         //
};
static const ParamType paramsSI[] = {
    PARAM_RADIO,         //
    PARAM_GAIN,          //
    PARAM_BANDWIDTH,     //
    PARAM_SQUELCH_VALUE, //
    PARAM_MODULATION,    //
    PARAM_STEP,          //
    PARAM_VOLUME,        //
};

static const ParamType paramsBK1080[] = {
    PARAM_RADIO, //
    PARAM_STEP,  //
};

static const ParamType *radioParams[] = {
    [RADIO_BK4819] = paramsBK4819,
    [RADIO_SI4732] = paramsSI,
    [RADIO_BK1080] = paramsBK1080,
};

static const uint8_t radioParamCount[] = {
    [RADIO_BK4819] = ARRAY_SIZE(paramsBK4819),
    [RADIO_SI4732] = ARRAY_SIZE(paramsSI),
    [RADIO_BK1080] = ARRAY_SIZE(paramsBK1080),
};

static void initMenu() {
  VFOContext *ctx = &RADIO_GetCurrentVFO(gRadioState)->context;
  regsMenu.num_items = radioParamCount[ctx->radio_type];
  for (uint8_t i = 0; i < regsMenu.num_items; ++i) {
    ParamType p = radioParams[ctx->radio_type][i];
    menuItems[i].name = PARAM_NAMES[p];
    menuItems[i].setting = p;
    menuItems[i].change_value = updateVal;
    menuItems[i].get_value_text = getValS;
  }
  MENU_Init(&regsMenu);
}

void REGSMENU_Draw() {
  if (inMenu) {
    MENU_Render();
  }
}

bool REGSMENU_Key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_0:
      inMenu = !inMenu;
      if (inMenu) {
        initMenu();
      } else {
        MENU_Deinit();
      }
      return true;
    case KEY_EXIT:
      if (inMenu) {
        inMenu = false;
        MENU_Deinit();
        return true;
      }
      break;
    default:
      break;
    }
  }
  if (inMenu && MENU_HandleInput(key, state)) {
    return true;
  }

  return inMenu;
}
