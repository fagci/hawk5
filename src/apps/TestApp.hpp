#pragma once

#include "../driver/K5.hpp"
#include "../driver/systick.h"
#include "../ui/graphics.h"
#include "App.hpp"

class TestApp final : public App {
public:
  TestApp() {}

  void init() override { BK4819_Init(); }

  void render() override {}

  void update() override {}

  void deinit() override {}

  uint8_t getAppId() const override { return APP_TEST; }
  const char *getName() const override { return "Test"; }

private:
  int rawKey;
};
