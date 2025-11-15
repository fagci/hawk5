#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "bk1080.h"
#include "bk4819-regs.h"
#include "bk4819.h"
#include <cstring>

class BK4819Driver : public IRadioDriver {
public:
  BK4819Driver() : IRadioDriver() {
    // ====================================================================
    // FREQUENCY - контекстно-зависимые границы
    // ====================================================================
    params_[(uint8_t)ParamId::Frequency] = Param<std::function<void(uint32_t)>>(
        "Freq", 145500000,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            BK4819_TuneTo(v, false);
            BK4819_SelectFilterEx((v < SETTINGS_GetFilterBound()) ? FILTER_VHF
                                                                  : FILTER_UHF);
          }

          // АВТО-ПЕРЕКЛЮЧЕНИЕ МОДУЛЯЦИИ
          // FM broadcast диапазон: 88-108 МГц
          if (v >= 8800000 && v <= 10800000) {
            params_[(uint8_t)ParamId::Modulation].set(MOD_WFM);
          }
          // VHF/UHF - FM по умолчанию
          else if (v >= 13000000) {
            if (params_[(uint8_t)ParamId::Modulation].get() == MOD_WFM) {
              params_[(uint8_t)ParamId::Modulation].set(MOD_FM);
            }
          }
          // КВ - AM по умолчанию
          else if (v < 3000000) {
            if (params_[(uint8_t)ParamId::Modulation].get() == MOD_WFM ||
                params_[(uint8_t)ParamId::Modulation].get() == MOD_FM) {
              params_[(uint8_t)ParamId::Modulation].set(MOD_AM);
            }
          }
        },
        []() { return BK4819_F_MIN; }, // min - статический
        []() { return BK4819_F_MAX; }, // max - статический
        PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // MODULATION - контекстно-зависимые границы
    // ====================================================================
    params_[(uint8_t)ParamId::Modulation] =
        Param<std::function<void(uint32_t)>>(
            "Mod", MOD_FM,
            [this](uint32_t v) {
              if (powerState_ > 0)
                BK4819_SetModulation((ModulationType)v);
            },
            // min - всегда 0
            []() { return 0u; },
            // max - зависит от частоты!
            [this]() -> uint32_t {
              uint32_t freq = params_[(uint8_t)ParamId::Frequency].get();

              // FM broadcast: только WFM
              if (freq >= 8800000 && freq <= 10800000) {
                return MOD_WFM; // min=max=WFM → только одна модуляция
              }
              // VHF/UHF: FM, AM, BYP, RAW
              else if (freq >= 13000000) {
                return MOD_RAW;
              }
              // КВ: все кроме WFM
              else {
                return MOD_RAW;
              }
            },
            PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // BANDWIDTH - контекстно-зависимый
    // ====================================================================
    params_[(uint8_t)ParamId::Bandwidth] = Param<std::function<void(uint32_t)>>(
        "BW", 1,
        [this](uint32_t v) {
          if (powerState_ > 0)
            BK4819_SetFilterBandwidth((BK4819_FilterBandwidth_t)v);
        },
        []() { return 0u; },
        [this]() -> uint32_t {
          uint32_t mod = params_[(uint8_t)ParamId::Modulation].get();
          // WFM - широкий диапазон полос
          if (mod == MOD_WFM)
            return 7;
          // Узкие режимы - ограниченный выбор
          if (mod == MOD_USB || mod == MOD_LSB)
            return 3;
          return 7;
        },
        PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    // ====================================================================
    // GAIN - контекстно-зависимый
    // ====================================================================
    params_[(uint8_t)ParamId::Gain] = Param<std::function<void(uint32_t)>>(
        "Gain", 16,
        [this](uint32_t v) {
          if (powerState_ > 0) {
            BK4819_SetAGC(params_[(uint8_t)ParamId::Modulation].get() != MOD_AM,
                          v);
          }
        },
        // min - зависит от модуляции
        [this]() -> uint32_t {
          uint32_t mod = params_[(uint8_t)ParamId::Modulation].get();
          // WFM - минимум выше для защиты от перегрузки
          if (mod == MOD_WFM)
            return 8;
          return 0;
        },
        []() { return 31u; }, PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    params_[(uint8_t)ParamId::Squelch] = Param<std::function<void(uint32_t)>>(
        "SQL", 4,
        [this](uint32_t v) {
          if (powerState_ > 0)
            BK4819_Squelch(v, 100, 200);
        },
        0, 9, PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    params_[(uint8_t)ParamId::Volume] = Param<std::function<void(uint32_t)>>(
        "Vol", 8,
        [this](uint32_t v) {
          if (powerState_ > 0)
            BK4819_SetRegValue(RS_AF_RX_GAIN, v);
        },
        0, 15, PARAM_READABLE | PARAM_WRITABLE | PARAM_PERSIST);

    params_[(uint8_t)ParamId::Step] = Param<std::function<void(uint32_t)>>(
        nullptr, 12500, [](uint32_t) {}, 0, 1000000,
        PARAM_READABLE | PARAM_WRITABLE);

    params_[(uint8_t)ParamId::PowerOn] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t on) {
          (on ? BK4819_Init : BK4819_Idle)();
          powerState_ = on;
        },
        0, 1, PARAM_READABLE | PARAM_WRITABLE);

    params_[(uint8_t)ParamId::RxMode] = Param<std::function<void(uint32_t)>>(
        nullptr, 0,
        [this](uint32_t v) {
          if (v) {
            // Включаем приём
            if (powerState_ == 0) {
              params_[(uint8_t)ParamId::PowerOn].set(1);
            }
            BK4819_RX_TurnOn();
            powerState_ = 1;

            // ПРИМЕНЯЕМ ВСЕ НАСТРОЙКИ после включения
            applyToHardware();
          } else {
            BK4819_Idle();
          }
        },
        0, 1, PARAM_READABLE | PARAM_WRITABLE);

    // READ ONLY PARAMS
    // RSSI - только чтение
    params_[(uint8_t)ParamId::RSSI] = Param<std::function<void(uint32_t)>>(
        "RSSI", // Имя
        0,      // Начальное значение
        [](uint32_t) {}, // Пустая лямбда apply (ничего не делает)
        0,               // min
        65535,         // max
        PARAM_READABLE // Только READABLE, без WRITABLE!
    );

    // Noise - только чтение
    params_[(uint8_t)ParamId::Noise] = Param<std::function<void(uint32_t)>>(
        "Noise", 0, [](uint32_t) {}, 0, 255, PARAM_READABLE);

    // SNR - только чтение
    params_[(uint8_t)ParamId::SNR] = Param<std::function<void(uint32_t)>>(
        "SNR", 0, [](uint32_t) {}, 0, 127, PARAM_READABLE);

    // SquelchOpen - только чтение, bool как 0/1
    params_[(uint8_t)ParamId::SquelchOpen] =
        Param<std::function<void(uint32_t)>>(
            nullptr, // Без имени (не для меню)
            0, [](uint32_t) {}, 0, 1, PARAM_READABLE);
  }

  RadioType getRadioType() const override { return RadioType::BK4819; }
  const char *getRadioName() const override { return "BK4819"; }

  // operator[] - обновляем измерения перед возвратом
  ParamProxyFunc operator[](ParamId id) override {
    uint8_t idx = (uint8_t)id;

    // ОБНОВЛЯЕМ ИЗМЕРЕНИЯ перед чтением
    if (id == ParamId::RSSI) {
      params_[idx].setQuiet(BK4819_GetRSSI());
    }

    if (id == ParamId::Noise) {
      params_[idx].setQuiet(BK4819_GetNoise());
    }

    if (id == ParamId::Glitch) {
      params_[idx].setQuiet(BK4819_GetGlitch());
    }

    if (id == ParamId::SNR) {
      params_[idx].setQuiet(BK4819_GetSNR());
    }

    if (id == ParamId::SquelchOpen) {
      params_[idx].setQuiet(BK4819_IsSquelchOpen());
    }

    // Возвращаем proxy с актуальным значением
    return IRadioDriver::operator[](id);
  }
};
