#pragma once

#include "../radio.h"
#include "App.hpp"
#include <stddef.h>

class AppManager {
public:
  static AppManager &instance() {
    static AppManager inst;
    return inst;
  }

  void registerApp(App *app);
  void run(uint8_t appId);
  void exit();

  void update();
  void render();
  bool handleKey(KEY_Code_t key, Key_State_t state);

  App *getCurrentApp() { return currentApp_; }
  uint8_t getCurrentAppId() const { return currentAppId_; }


private:
  AppManager() = default;

  static constexpr size_t MAX_APPS = 16;
  App *apps_[MAX_APPS] = {nullptr};
  size_t appCount_ = 0;

  App *currentApp_ = nullptr;
  uint8_t currentAppId_ = 0xFF;
};
