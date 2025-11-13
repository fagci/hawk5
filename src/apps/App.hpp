#pragma once

#include "../driver/keyboard.h"
#include "../radio.h"
#include <stdint.h>
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

class App {
public:
  virtual ~App() = default;

  // Жизненный цикл приложения
  virtual void init() {}
  virtual void deinit() {}
  virtual void update() {}
  virtual void render() = 0; // Обязательно

  // Обработка клавиш
  virtual bool key(KEY_Code_t key, Key_State_t state) {
    (void)key;
    (void)state;
    return false;
  }

  // Метаданные
  virtual const char *getName() const = 0;
  virtual uint8_t getAppId() const = 0;
  virtual RadioState *getRadioState() { return nullptr; }
};

class RadioApp : public App {
public:
  RadioState radioState;
  RadioState *getRadioState() override { return &radioState; }
};
