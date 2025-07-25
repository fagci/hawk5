#ifndef APPS_H
#define APPS_H

#include "../driver/keyboard.h"
#include "../radio.h"

#define RUN_APPS_COUNT 8

typedef enum {
  APP_NONE,
  APP_SCANER,
  APP_CH_SCAN,
  APP_BAND_SCAN,
  APP_FC,
  APP_CH_LIST,
  APP_FINPUT,
  APP_APPS_LIST,
  APP_LOOT_LIST,
  APP_RESET,
  APP_TEXTINPUT,
  APP_CH_CFG,
  APP_SETTINGS,
  APP_VFO1,
  APP_ABOUT,

  APPS_COUNT,
} AppType_t;

typedef struct App {
  const char *name;
  void (*init)(void);
  void (*update)(void);
  void (*render)(void);
  bool (*key)(KEY_Code_t Key, Key_State_t state);
  void (*deinit)(void);
  bool needsRadioState;
} App;

extern const App apps[APPS_COUNT];
extern const AppType_t appsAvailableToRun[RUN_APPS_COUNT];

extern AppType_t gCurrentApp;

AppType_t APPS_Peek();
bool APPS_key(KEY_Code_t Key, Key_State_t state);
void APPS_init(AppType_t app);
void APPS_update(void);
void APPS_render(void);
void APPS_run(AppType_t app);
void APPS_runManual(AppType_t app);
bool APPS_exit(void);

#endif /* end of include guard: APPS_H */
