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
    /* vfoBank.loadAll();
    vfoBank.setActive(gSettings.activeVFO); */
    uint8_t vfoIdx = 0;
    for (uint16_t i = 0; i < CHANNELS_GetCountMax(); ++i) {
      CHMeta meta = CHANNELS_GetMeta(i);

      bool isOurType = (TYPE_FILTER_VFO & (1 << meta.type)) != 0;
      if (!isOurType) {
        continue;
      }

      vfoBank.loadChannelAuto(vfoIdx, i);
      vfoIdx++;
    }
    vfoBank.setActiveVFO(gSettings.activeVFO);
    vfoBank.powerOnAndReceive();
  }

  void render() override {
    UI_BigFrequency(42, vfoBank[ParamId::Frequency]);
    PrintMedium(0, 16, "RSSI: %u", vfoBank[ParamId::RSSI]);
    PrintMedium(0, 24, "SQ OP: %u", vfoBank[ParamId::SquelchOpen]);
    PrintMedium(0, 32, "VFO %u", vfoBank.getActiveVFOIndex() + 1);
  }

  void update() override { vfoBank.updateMeasurements(); }

  bool key(KEY_Code_t key, Key_State_t state) override {
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
        vfoBank.switchVFO(IncDecU(vfoBank.getActiveVFOIndex(), 0, 4, true));
        // Переход в standby
        /* if (vfo->isRxActive()) {
          vfoBank.setActiveStandby();
        } else {
          vfoBank.powerOnActive();
        } */
        return true;

      case KEY_MENU:
        // Mute toggle
        vfoBank[ParamId::Mute] = !vfoBank[ParamId::Mute];
        return true;
      case KEY_UP:
        vfoBank[ParamId::Frequency] += 25.0_kHz;
        return true;

      case KEY_DOWN:
        vfoBank[ParamId::Frequency] -= 25.0_kHz;
        return true;

        /* case KEY_0 ... KEY_9:
          inputOverlay.open(key * MHZ, 0, 1300 * MHZ, InputUnit::MHz,
                            [&](uint32_t value) {
                              vfo[ParamId::Frequency] = value;
                              // сразу применить — если нужно
                              // vfo[ParamId::];
                            });
          return true; */
      case KEY_1:
      case KEY_2:
      case KEY_3:
      case KEY_4:
        // Переключение VFO
        vfoBank.switchVFO(key - KEY_1);
        vfoBank.dumpState();
        return true;

      case KEY_STAR:
        // Старт/стоп сканирования
        /* if (vfoBank.isScanning()) {
          vfoBank.stopScan();
        } else {
          vfoBank.startScan(vfoBank.getActiveVFOIndex());
        } */
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
