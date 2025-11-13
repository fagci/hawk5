#include "AppManager.hpp"
#include "../driver/uart.h"
#include "../external/printf/printf.h"
#include "App.hpp"

void AppManager::registerApp(App *app) {
  if (appCount_ < MAX_APPS) {
    apps_[appCount_++] = app;
  }
}

void AppManager::run(uint8_t appId) {
  // Найти приложение
  App *newApp = nullptr;
  for (size_t i = 0; i < appCount_; i++) {
    if (apps_[i]->getAppId() == appId) {
      newApp = apps_[i];
      break;
    }
  }

  if (!newApp) {
    printf("App %d not found\n", appId);
    return;
  }

  // Деинициализировать текущее
  if (currentApp_) {
    currentApp_->deinit();
  }

  // Инициализировать новое
  currentApp_ = newApp;
  currentAppId_ = appId;
  currentApp_->init();

  if (RadioState *state = currentApp_->getRadioState()) {
    RADIO_ToggleMultiwatch(gRadioState, gSettings.mWatch);
  }

  printf("App changed to: %s\n", currentApp_->getName());
}

void AppManager::exit() {
  if (currentApp_) {
    currentApp_->deinit();
    currentApp_ = nullptr;
  }
}

void AppManager::update() {
  if (currentApp_) {
    currentApp_->update();
  }
}

void AppManager::render() {
  if (currentApp_) {
    currentApp_->render();
  }
}

bool AppManager::handleKey(KEY_Code_t key, Key_State_t state) {
  if (currentApp_) {
    return currentApp_->key(key, state);
  }
  return false;
}
