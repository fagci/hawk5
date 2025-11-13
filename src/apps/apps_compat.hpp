#include "AppManager.hpp"
#include "TestApp.hpp"
#include "VFOApp.hpp"

// Глобальные экземпляры приложений
static VFOApp vfoApp;
static TestApp testApp;

extern "C" {

void APPS_Init() {
  auto &mgr = AppManager::instance();
  mgr.registerApp(&vfoApp);
  mgr.registerApp(&testApp);
}

void APPS_run(uint8_t appId) { AppManager::instance().run(appId); }

void APPS_exit() { AppManager::instance().exit(); }

void APPS_update() { AppManager::instance().update(); }

void APPS_render() { AppManager::instance().render(); }

bool APPS_key(KEY_Code_t key, Key_State_t state) {
  return AppManager::instance().handleKey(key, state);
}

uint8_t APPS_GetCurrent() { return AppManager::instance().getCurrentAppId(); }

} // extern "C"
