#include "AppManager.hpp"
#include "VFOApp.hpp"

// Глобальные экземпляры приложений
static VFOApp vfoApp;

extern "C" {

void APPS_Init() {
  printf("sizeof(VFOApp): %u\n", sizeof(VFOApp));
  printf("=== APPS_Init START ===\n");

  auto &mgr = AppManager::instance();

  printf("Registering VFO app...\n");
    printf("VFOApp getName: %s\n", vfoApp.getName());
    printf("VFOApp getAppId: %u\n",vfoApp.getAppId());
  mgr.registerApp(&vfoApp);

  printf("=== APPS_Init DONE ===\n");
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
