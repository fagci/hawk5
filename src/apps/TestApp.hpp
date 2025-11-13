#pragma once

#include "../parameters.hpp"
#include "../ui/graphics.h"
#include "App.hpp"

class TestApp final : public App {
public:
  void init() override {
    BK4819_Init();
    BK4819_RX_TurnOn();
    BK4819_SetAGC(true, AUTO_GAIN_INDEX);
    BK4819_TuneTo(172.3_MHz, true);
    BK4819_SetModulation(MOD_FM);
    BK4819_SetFilterBandwidth(BK4819_FILTER_BW_12k);
    BK4819_SelectFilterEx((BK4819_GetFrequency() < SETTINGS_GetFilterBound())
                              ? FILTER_VHF
                              : FILTER_UHF);
  }

  void render() override {
    for (uint8_t i = 0; i < ARRAY_SIZE(radioParams); ++i) {
      const Param *p = &radioParams[i];
      PrintSmall(0, 16 + i * 6, "%s: %u", p->name, p->getValue());
    }
  }

  uint8_t getAppId() const override { return APP_TEST; }
  const char *getName() const override { return "Test"; }
};
