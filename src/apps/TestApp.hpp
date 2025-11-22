#pragma once

#include "../board.h"
#include "../driver/K5.hpp"
#include "../driver/audio.h"
#include "../driver/systick.h"
#include "../scheduler.h"
#include "../ui/NumberInputOverlay.hpp"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "App.hpp"

class TestApp final : public App {
public:
  TestApp() {}

  void init() override {
    BK4819_Init();
    BK4819_RX_TurnOn();

    K5::LNA::select(FILTER_UHF);
    BK4819_TuneTo(f, true);
    BK4819_SetModulation(MOD_FM);
    BK4819_SetFilterBandwidth(BK4819_FILTER_BW_12k);

    BK4819_Squelch(3, 0, 0);
  }

  void render() override {
    if (K5::numberInput.isActive()) {
      K5::numberInput.render();
    }
    // UI_BigFrequency(32, f);
    PrintMedium(0, 16, "RSSI: %u", BK4819_GetRSSI());
    PrintMedium(0, 24, "AGC: %u", BK4819_GetAgcLevel());
    PrintMedium(0, 32, "LNA P R: %u", BK4819_GetLnaPeakRSSI());
  }

  void update() override {
    bool on = BK4819_IsSquelchOpen();
    K5::Audio::write(on);
  }

  uint8_t getAppId() const override { return APP_TEST; }
  const char *getName() const override { return "Test"; }

private:
  uint32_t f = 43422500;
};
