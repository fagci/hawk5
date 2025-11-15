#pragma once

#include "../driver/VFOBank.hpp"
#include "../driver/uart.h"
#include "../ui/NumberInputOverlay.hpp"
#include "../ui/TextInputOverlay.hpp"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "App.hpp"

class TestApp final : public App {
public:
  void init() override {
    vfoBank.loadAll();
    vfoBank.setActive(gSettings.activeVFO);
  }

  void render() override {
    auto vfo = vfoBank.active();
    UI_BigFrequency(42, vfo[ParamId::Frequency]);
    PrintMedium(0, 16, "RSSI: %u", vfo[ParamId::RSSI].get());
    // PrintMedium(0, 24, "SQ OP: %u", vfo->isSquelchOpen());
  }

  void update() override { vfoBank.scanTick(); }

  bool key(KEY_Code_t key, Key_State_t state) override {
    auto vfo = vfoBank.active();

    if (inputOverlay.isActive()) {
      inputOverlay.handleKey(key, state);
      return true;
    }
    if (textInput.isActive()) {
      textInput.handleKey(key, state);
      return true;
    }
    if (state == KEY_RELEASED) {

      switch (key) {
      case KEY_EXIT:
        // Переход в standby
        /* if (vfo->isRxActive()) {
          vfoBank.setActiveStandby();
        } else {
          vfoBank.powerOnActive();
        } */
        return true;

      case KEY_MENU:
        // Mute toggle
        vfo[ParamId::Mute] = !vfo[ParamId::Mute];
        return true;
      case KEY_UP:
        vfo[ParamId::Frequency] += 25.0_kHz;
        return true;

      case KEY_DOWN:
        vfo[ParamId::Frequency] -= 25.0_kHz;
        return true;

      case KEY_0 ... KEY_9:
        inputOverlay.open(key * MHZ, 0, 1300 * MHZ, InputUnit::MHz,
                          [&](uint32_t value) {
                            vfo[ParamId::Frequency] = value;
                            // сразу применить — если нужно
                            // vfo[ParamId::];
                          });
        return true;
        /* case KEY_1:
        case KEY_2:
        case KEY_3:
        case KEY_4:
          // Переключение VFO
          vfoBank.setActive(key - KEY_1);
          return true; */

      case KEY_STAR:
        // Старт/стоп сканирования
        if (vfoBank.isScanning()) {
          vfoBank.stopScan();
        } else {
          vfoBank.startScan();
        }
        return true;

        /* case KEY_5:
          // Сохранить
          vfoBank.saveAll();
          return true; */

      default:
        break;
      }
    }
    return false;
  }

  void deinit() override {}

  uint8_t getAppId() const override { return APP_TEST; }

  const char *getName() const override { return "Test"; }

private:
  bool signalDetected_ = false;

  NumberInputOverlay inputOverlay;
  TextInputOverlay textInput;

  VFOBank vfoBank;
};
