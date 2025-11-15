#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "si473x.h"

class SI4732Driver : public IRadioDriver {
public:
  SI4732Driver() : IRadioDriver() {
    // ====================================================================
    // FREQUENCY
    // ====================================================================
    params_[(uint8_t)ParamId::Frequency] = Param<std::function<void(uint32_t)>>(
        "Freq", 10000000,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            SI47XX_TuneTo(v);
          }

          // Auto mode switching based on frequency
          SI47XX_MODE newMode = SI47XX_FM;
          if (v < 3000000) {
            // < 30 MHz - AM or SSB
            uint32_t mod = params_[(uint8_t)ParamId::Modulation].get();
            if (mod == MOD_USB)
              newMode = SI47XX_USB;
            else if (mod == MOD_LSB)
              newMode = SI47XX_LSB;
            else
              newMode = SI47XX_AM;
          } else if (v < 7600000) {
            newMode = SI47XX_AM;
          } else {
            // FM broadcast
            newMode = SI47XX_FM;
            params_[(uint8_t)ParamId::Modulation].set(MOD_WFM);
          }

          if (si4732mode != newMode) {
            SI47XX_SwitchMode(newMode);
          }
        },
        []() { return 15000u; },    // 150 kHz min
        []() { return 10800000u; }, // 108 MHz max
        PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // MODULATION
    // ====================================================================
    params_[(uint8_t)ParamId::Modulation] =
        Param<std::function<void(uint32_t)>>(
            "Mod", MOD_FM,
            [this](uint32_t v) {
              uint32_t freq = params_[(uint8_t)ParamId::Frequency].get();
              SI47XX_MODE newMode = SI47XX_FM;

              if (v == MOD_WFM && freq >= 7600000) {
                newMode = SI47XX_FM;
              } else if (v == MOD_AM && freq < 3000000) {
                newMode = SI47XX_AM;
              } else if (v == MOD_USB && freq < 3000000) {
                newMode = SI47XX_USB;
              } else if (v == MOD_LSB && freq < 3000000) {
                newMode = SI47XX_LSB;
              }

              if (si4732mode != newMode && powerState_ > 0) {
                SI47XX_SwitchMode(newMode);
              }
            },
            []() { return 0u; },
            [this]() -> uint32_t {
              uint32_t freq = params_[(uint8_t)ParamId::Frequency].get();
              if (freq >= 7600000)
                return MOD_WFM; // FM only
              if (freq >= 3000000)
                return MOD_AM; // AM only
              return MOD_LSB;  // AM/USB/LSB
            },
            PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // BANDWIDTH
    // ====================================================================
    params_[(uint8_t)ParamId::Bandwidth] = Param<std::function<void(uint32_t)>>(
        "BW", 1,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            if (SI47XX_IsSSB()) {
              SI47XX_SetSsbBandwidth((SI47XX_SsbFilterBW)v);
            } else {
              SI47XX_SetBandwidth((SI47XX_FilterBW)v, v > 3);
            }
          }
        },
        []() { return 0u; },
        [this]() -> uint32_t { return SI47XX_IsSSB() ? 5u : 6u; },
        PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // GAIN (AGC Index)
    // ====================================================================
    params_[(uint8_t)ParamId::Gain] = Param<std::function<void(uint32_t)>>(
        "Gain", 0,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            SI47XX_SetAutomaticGainControl(v > 0 ? 1 : 0, v);
          }
        },
        []() { return 0u; }, []() { return 37u; },
        PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // VOLUME
    // ====================================================================
    params_[(uint8_t)ParamId::Volume] = Param<std::function<void(uint32_t)>>(
        "Vol", 32,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            SI47XX_SetVolume(v);
          }
        },
        0, 63, PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    params_[(uint8_t)ParamId::Squelch] = Param<std::function<void(uint32_t)>>(
        "SQL", 10,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            if (si4732mode == SI47XX_FM) {
              SI47XX_SetSeekFmRssiThreshold(v);
            } else {
              SI47XX_SetSeekAmRssiThreshold(v);
            }
          }
        },
        0, 127, PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    params_[(uint8_t)ParamId::Step] = Param<std::function<void(uint32_t)>>(
        nullptr, 10000, [](uint32_t) {}, 1000, 100000,
        PARAM_READABLE | PARAM_WRITABLE);

    // ====================================================================
    // MEASUREMENTS
    // ====================================================================
    params_[(uint8_t)ParamId::RSSI] = Param<std::function<void(uint32_t)>>(
        "RSSI", 0, [](uint32_t) {}, 0, 127, PARAM_READABLE);

    params_[(uint8_t)ParamId::SNR] = Param<std::function<void(uint32_t)>>(
        "SNR", 0, [](uint32_t) {}, 0, 127, PARAM_READABLE);

    params_[(uint8_t)ParamId::SquelchOpen] =
        Param<std::function<void(uint32_t)>>(
            nullptr, 0, [](uint32_t) {}, 0, 1, PARAM_READABLE);

    // ====================================================================
    // STATE
    // ====================================================================
    params_[(uint8_t)ParamId::RxMode] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t v) {
          if (v && powerState_ == 0) {
            params_[(uint8_t)ParamId::PowerOn].set(1);
          }
        },
        0, 1, PARAM_READABLE | PARAM_WRITABLE);

    params_[(uint8_t)ParamId::Mute] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            SI47XX_SetVolume(v ? 0 : params_[(uint8_t)ParamId::Volume].get());
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
            if (SI47XX_IsSSB()) {
              SI47XX_PatchPowerUp();
            } else {
              SI47XX_PowerUp();
            }
            powerState_ = 1;
          }
        },
        0, 0, PARAM_ACTION);

    params_[(uint8_t)ParamId::PowerOff] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t) {
          if (powerState_ > 0) {
            SI47XX_PowerDown();
            powerState_ = 0;
          }
        },
        0, 0, PARAM_ACTION);
  }

  RadioType getRadioType() const override { return RadioType::SI4732; }
  const char *getRadioName() const override { return "SI4732"; }

  ParamProxy<std::function<void(uint32_t)>> operator[](ParamId id) override {
    uint8_t idx = (uint8_t)id;

    // Update measurements
    if (id == ParamId::RSSI || id == ParamId::SNR ||
        id == ParamId::SquelchOpen) {
      RSQ_GET();
      params_[(uint8_t)ParamId::RSSI].set(rsqStatus.resp.RSSI);
      params_[(uint8_t)ParamId::SNR].set(rsqStatus.resp.SNR);
      params_[(uint8_t)ParamId::SquelchOpen].set(rsqStatus.resp.SMUTE ? 0 : 1);
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
