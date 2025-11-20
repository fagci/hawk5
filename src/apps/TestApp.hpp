#pragma once

#include "../driver/K5.hpp"
#include "../driver/systick.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "App.hpp"

class TestApp final : public App {
public:
  TestApp() {}

  void init() override {
    BK4819_Init();
    K5::vfo.setActiveVFO(0);
    K5::vfo.switchReceiver();
    K5::vfo.setFrequency(434 * MHZ);
  }

  void render() override { UI_BigFrequency(32, K5::vfo.getFrequency()); }

  void update() override {}

  uint8_t getAppId() const override { return APP_TEST; }
  const char *getName() const override { return "Test"; }

private:
  int rawKey;
};
