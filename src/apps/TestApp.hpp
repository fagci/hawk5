#pragma once

#include "../driver/K5.hpp"
#include "../driver/systick.h"
#include "../ui/graphics.h"
#include "App.hpp"

class TestApp final : public App {
public:
  TestApp() {}

  void init() override {
    BK4819_Init();
    K5::Keyboard::init();
  }

  void render() override {
    PrintMedium(0, 16, "PTT: %u", K5::Button::isPTTPressed());
    PrintMedium(0, 24, "Key: %d", rawKey);
  }

  void update() override {
    rawKey = K5::Keyboard::scan();
    kbd.update();
  }

  bool key(KEY_Code_t key, Key_State_t state) override { return false; }

  void deinit() override {}

  uint8_t getAppId() const override { return APP_TEST; }
  const char *getName() const override { return "Test"; }

private:
  int rawKey;
  K5::KeyboardController kbd;
};
