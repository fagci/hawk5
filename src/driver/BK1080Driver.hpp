#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "bk1080.h"

class BK1080Driver : public IRadioDriver {
public:
  BK1080Driver() : IRadioDriver() {
    // ====================================================================
    // FREQUENCY - FM только 64-108 МГц
    // ====================================================================
    params_[(uint8_t)ParamId::Frequency] = Param<std::function<void(uint32_t)>>(
        "Freq", 10000000,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            BK1080_SetFrequency(v);
          }
        },
        6400000,  // 64 MHz
        10800000, // 108 MHz
        PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // MODULATION - только WFM
    // ====================================================================
    params_[(uint8_t)ParamId::Modulation] =
        Param<std::function<void(uint32_t)>>(
            "Mod", MOD_WFM, [](uint32_t) {}, // Не меняется
            MOD_WFM, MOD_WFM,                // min=max=WFM
            PARAM_READABLE);

    // ====================================================================
    // VOLUME - через регистр
    // ====================================================================
    params_[(uint8_t)ParamId::Volume] = Param<std::function<void(uint32_t)>>(
        "Vol", 15,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            uint16_t reg5 =
                BK1080_ReadRegister(BK1080_REG_05_SYSTEM_CONFIGURATION2);
            reg5 = (reg5 & 0xFFF0) | (v & 0x0F);
            BK1080_WriteRegister(BK1080_REG_05_SYSTEM_CONFIGURATION2, reg5);
          }
        },
        0, 15, PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    params_[(uint8_t)ParamId::Step] = Param<std::function<void(uint32_t)>>(
        nullptr, 10000, [](uint32_t) {}, 5000, 20000,
        PARAM_READABLE | PARAM_WRITABLE);

    // ====================================================================
    // MEASUREMENTS
    // ====================================================================
    params_[(uint8_t)ParamId::RSSI] = Param<std::function<void(uint32_t)>>(
        "RSSI", 0, [](uint32_t) {}, 0, 511, PARAM_READABLE);

    params_[(uint8_t)ParamId::SNR] = Param<std::function<void(uint32_t)>>(
        "SNR", 0, [](uint32_t) {}, 0, 15, PARAM_READABLE);

    // ====================================================================
    // STATE
    // ====================================================================
    params_[(uint8_t)ParamId::Mute] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            BK1080_Mute(v);
          }
        },
        0, 1, PARAM_READABLE | PARAM_WRITABLE);

    // ====================================================================
    // ACTIONS
    // ====================================================================
    params_[(uint8_t)ParamId::PowerOn] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t) {
          if (powerState_ == 0) {
            BK1080_Init(params_[(uint8_t)ParamId::Frequency].get(), true);
            powerState_ = 1;
          }
        },
        0, 0, PARAM_ACTION);

    params_[(uint8_t)ParamId::PowerOff] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t) {
          if (powerState_ > 0) {
            BK1080_Init(0, false);
            powerState_ = 0;
          }
        },
        0, 0, PARAM_ACTION);

    params_[(uint8_t)ParamId::RxMode] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t v) {
          if (v && powerState_ == 0) {
            params_[(uint8_t)ParamId::PowerOn].set(1);
          }
        },
        0, 1, PARAM_READABLE | PARAM_WRITABLE);
  }

  RadioType getRadioType() const override { return RadioType::BK1080; }
  const char *getRadioName() const override { return "BK1080"; }

  ParamProxy<std::function<void(uint32_t)>> operator[](ParamId id) override {
    uint8_t idx = (uint8_t)id;

    // Update measurements
    if (powerState_ > 0) {
      if (id == ParamId::RSSI) {
        params_[idx].set(BK1080_GetRSSI());
      }
      if (id == ParamId::SNR) {
        params_[idx].set(BK1080_GetSNR());
      }
    }

    return params_[idx].proxy();
  }

  bool isDirty() const override {
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      if (params_[i].isDirty() && (params_[i].flags() & PARAM_PERSIST)) {
        return true;
      }
    }
    return false;
  }
};
