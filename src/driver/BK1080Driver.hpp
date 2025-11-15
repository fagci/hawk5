#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "bk1080.h"

// ============================================================================
// BK1080 DRIVER (FM-только приёмник 64-108 МГц)
// ============================================================================

class BK1080Driver : public IRadioDriver {
public:
  BK1080Driver() : IRadioDriver() {

    // ====================================================================
    // FREQUENCY - только FM диапазон 64-108 МГц
    // ====================================================================
    params_[(uint8_t)ParamId::Frequency] = Param<uint32_t>(
        10000000, // 100 MHz по умолчанию
        6400000,  // 64 MHz min
        10800000, // 108 MHz max
        this,
        [](void* ctx, uint32_t v) {
          auto* driver = static_cast<BK1080Driver*>(ctx);
          if (driver->powerState_ == 0) return;
          BK1080_SetFrequency(v);
        },
        PARAM_PERSIST
    );

    // ====================================================================
    // MODULATION - только WFM (фиксированная)
    // ====================================================================
    params_[(uint8_t)ParamId::Modulation] = Param<uint32_t>(
        (uint32_t)ModType::WFM,
        (uint32_t)ModType::WFM,
        (uint32_t)ModType::WFM,
        nullptr, nullptr, // Не изменяется
        PARAM_READONLY
    );

    // ====================================================================
    // VOLUME - 0-15
    // ====================================================================
    params_[(uint8_t)ParamId::Volume] = Param<uint32_t>(
        15, 0, 15,
        this,
        [](void* ctx, uint32_t v) {
          auto* driver = static_cast<BK1080Driver*>(ctx);
          if (driver->powerState_ == 0) return;

          uint16_t reg5 = BK1080_ReadRegister(BK1080_REG_05_SYSTEM_CONFIGURATION2);
          reg5 = (reg5 & 0xFFF0) | (v & 0x0F);
          BK1080_WriteRegister(BK1080_REG_05_SYSTEM_CONFIGURATION2, reg5);
        },
        PARAM_PERSIST
    );

    // ====================================================================
    // STEP - шаг частоты для FM
    // ====================================================================
    params_[(uint8_t)ParamId::Step] = Param<uint32_t>(
        10000, // 100 kHz по умолчанию
        5000,  // 50 kHz min
        20000, // 200 kHz max
        nullptr, nullptr,
        PARAM_PERSIST
    );

    // ====================================================================
    // READ-ONLY PARAMETERS (измерения)
    // ====================================================================
    params_[(uint8_t)ParamId::RSSI] = Param<uint32_t>(0, 0, 511, PARAM_READONLY);
    params_[(uint8_t)ParamId::SNR] = Param<uint32_t>(0, 0, 15, PARAM_READONLY);

    // ====================================================================
    // STATE
    // ====================================================================
    params_[(uint8_t)ParamId::Mute] = Param<uint32_t>(
        0, 0, 1,
        this,
        [](void* ctx, uint32_t v) {
          auto* driver = static_cast<BK1080Driver*>(ctx);
          if (driver->powerState_ == 0) return;
          BK1080_Mute(v);
        },
        0
    );

    params_[(uint8_t)ParamId::RxMode] = Param<uint32_t>(
        0, 0, 1,
        this,
        [](void* ctx, uint32_t v) {
          auto* driver = static_cast<BK1080Driver*>(ctx);
          if (v && driver->powerState_ == 0) {
            driver->getParam(ParamId::PowerOn).set(1);
          }
        },
        0
    );

    // ====================================================================
    // ACTIONS
    // ====================================================================
    params_[(uint8_t)ParamId::PowerOn] = Param<uint32_t>(
        0, 0, 1,
        this,
        [](void* ctx, uint32_t v) {
          auto* driver = static_cast<BK1080Driver*>(ctx);
          if (v && driver->powerState_ == 0) {
            driver->powerOn();
          }
        },
        PARAM_ACTION
    );

    params_[(uint8_t)ParamId::PowerOff] = Param<uint32_t>(
        0, 0, 1,
        this,
        [](void* ctx, uint32_t v) {
          auto* driver = static_cast<BK1080Driver*>(ctx);
          if (v && driver->powerState_ > 0) {
            driver->powerOff();
          }
        },
        PARAM_ACTION
    );
  }

  RadioType getRadioType() const override { return RadioType::BK1080; }
  const char* getRadioName() const override { return "BK1080"; }

  void powerOn() {
    if (powerState_ > 0) return;

    uint32_t freq = getParam(ParamId::Frequency).get();
    BK1080_Init(freq, true);
    powerState_ = 1;

    // Применяем параметры после включения
    getParam(ParamId::Volume).set(getParam(ParamId::Volume).get(), true);
  }

  void powerOff() {
    if (powerState_ == 0) return;
    BK1080_Init(0, false);
    powerState_ = 0;
  }

  // Обновление read-only параметров
  void updateMeasurements() {
    if (powerState_ == 0) return;

    getParam(ParamId::RSSI).set(BK1080_GetRSSI(), true);
    getParam(ParamId::SNR).set(BK1080_GetSNR(), true);
  }
};

