#include "apps.h"
#include "../driver/st7565.h"
#include "../driver/uart.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "about.h"
#include "appslist.h"
#include "chcfg.h"
#include "chlist.h"
/* #include "bandscan.h"
#include "chscan.h"
#include "fc.h" */
#include "finput.h"
// #include "lootlist.h"
#include "reset.h"
// #include "scaner.h"
#include "settings.h"
#include "textinput.h"
#include "vfo1.h"

#define APPS_STACK_SIZE 8

AppType_t gCurrentApp = APP_NONE;
RadioState gRadioState;

static AppType_t loadedVfoApp = APP_NONE;

static AppType_t appsStack[APPS_STACK_SIZE] = {APP_NONE};
static int8_t stackIndex = -1;

static bool pushApp(AppType_t app) {
  if (stackIndex < APPS_STACK_SIZE - 1) {
    appsStack[++stackIndex] = app;
  } else {
    for (uint8_t i = 1; i < APPS_STACK_SIZE; ++i) {
      appsStack[i - 1] = appsStack[i];
    }
    appsStack[stackIndex] = app;
  }
  return true;
}

static AppType_t popApp(void) {
  if (stackIndex > 0) {
    return appsStack[stackIndex--]; // Do not care about existing value
  }
  return appsStack[stackIndex];
}

AppType_t APPS_Peek(void) {
  if (stackIndex >= 0) {
    return appsStack[stackIndex];
  }
  return APP_NONE;
}

const AppType_t appsAvailableToRun[RUN_APPS_COUNT] = {
    APP_VFO1,    //
    APP_CH_LIST, //
    /*
    APP_SCANER,    //
    APP_CH_SCAN,   //
    APP_BAND_SCAN, //
    APP_FC,        //
    APP_LOOT_LIST, // */
    APP_ABOUT, //
};

const App apps[APPS_COUNT] = {
    [APP_NONE] = {"None", NULL, NULL, NULL, NULL, NULL},
    [APP_FINPUT] = {"Freq input", FINPUT_init, FINPUT_update, FINPUT_render,
                    FINPUT_key, FINPUT_deinit},
    [APP_TEXTINPUT] = {"Text input", TEXTINPUT_init, NULL, TEXTINPUT_render,
                       TEXTINPUT_key, TEXTINPUT_deinit},
    [APP_SETTINGS] = {"Settings", SETTINGS_init, NULL, SETTINGS_render,
                      SETTINGS_key, SETTINGS_deinit},
    [APP_APPS_LIST] = {"Run app", APPSLIST_init, NULL, APPSLIST_render,
                       APPSLIST_key, NULL},
    [APP_RESET] = {"Reset", RESET_Init, RESET_Update, RESET_Render, RESET_key,
                   NULL},
    [APP_CH_CFG] = {"CH cfg", CHCFG_init, NULL, CHCFG_render, CHCFG_key,
                    CHCFG_deinit},
    [APP_CH_LIST] = {"Channels", CHLIST_init, NULL, CHLIST_render, CHLIST_key,
                     CHLIST_deinit},
    /* [APP_SPECTRUM] = {"Spectrum", SCANER_init, SCANER_update, SCANER_render,
    SCANER_key, SCANER_deinit}, [APP_LOOT_LIST] = {"Loot", LOOTLIST_init,
    LOOTLIST_update, LOOTLIST_render, LOOTLIST_key, NULL}, [APP_CH_SCAN] = {"CH
    Scan", CHSCAN_init, CHSCAN_update, CHSCAN_render, CHSCAN_key,
    CHSCAN_deinit},
    [APP_BAND_SCAN] = {"Band Scan", BANDSCAN_init,
    BANDSCAN_update, BANDSCAN_render, BANDSCAN_key, BANDSCAN_deinit},
    [APP_FC] =
    {"FC", FC_init, FC_update, FC_render, FC_key, FC_deinit},
  */
    [APP_VFO1] = {"1 VFO", VFO1_init, VFO1_update, VFO1_render, VFO1_key, NULL,
                  true, true},
    [APP_ABOUT] = {"ABOUT", NULL, NULL, ABOUT_Render, ABOUT_key, NULL},
};

bool APPS_key(KEY_Code_t Key, Key_State_t state) {
  if (apps[gCurrentApp].key) {
    return apps[gCurrentApp].key(Key, state);
  }
  return false;
}

void APPS_init(AppType_t app) {

  STATUSLINE_SetText("%s", apps[gCurrentApp].name);
  gRedrawScreen = true;

  if (apps[gCurrentApp].init) {
    apps[gCurrentApp].init();
  }
}

void APPS_update(void) {
  if (apps[gCurrentApp].update) {
    apps[gCurrentApp].update();
  } else {
    gRedrawScreen = true;
  }
}

void APPS_render(void) {
  if (apps[gCurrentApp].render) {
    UI_ClearScreen();
    apps[gCurrentApp].render();
  }
}

void APPS_deinit(void) {
  if (apps[gCurrentApp].deinit) {
    apps[gCurrentApp].deinit();
  }
}

void APPS_run(AppType_t app) {
  if (appsStack[stackIndex] == app) {
    return;
  }
  /* if (app != APP_FINPUT && app != APP_TEXTINPUT) {
    APPS_deinit();
  } */
  pushApp(app);
  gCurrentApp = app;

  if (loadedVfoApp != gCurrentApp && apps[gCurrentApp].needsRadioState) {
    LogC(LOG_C_MAGENTA, "[APP] Load radio state for %s",
         apps[gCurrentApp].name);
    RADIO_InitState(&gRadioState, 16);
    RADIO_LoadVFOs(&gRadioState);
    loadedVfoApp = gCurrentApp;
  }

  APPS_init(app);
}

void APPS_runManual(AppType_t app) { APPS_run(app); }

bool APPS_exit(void) {
  if (stackIndex == 0) {
    return false;
  }
  APPS_deinit();
  AppType_t app = popApp();
  /* if (app != APP_FINPUT && app != APP_TEXTINPUT) {
    APPS_init(APPS_Peek());
  } else { */
  gCurrentApp = APPS_Peek();

  STATUSLINE_SetText("%s", apps[gCurrentApp].name);
  gRedrawScreen = true;
  // }
  return true;
}
