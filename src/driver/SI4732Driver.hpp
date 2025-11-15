#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "si473x.h"

// ============================================================================
// SI4732 DRIVER
// ============================================================================

class SI4732Driver : public IRadioDriver {
public:
  SI4732Driver()
      : IRadioDriver(), si4732mode_(SI47XX_FM), applyingFrequency_(false) {

    // ====================================================================
    // FREQUENCY - с автопереключением режима
    // ====================================================================
    params_[(uint8_t)ParamId::Frequency] = Param<uint32_t>(
        10000000, this,
        [](void *ctx) -> uint32_t { return 15000; },    // 150 kHz min
        [](void *ctx) -> uint32_t { return 10800000; }, // 108 MHz max
        [](void *ctx, uint32_t v) {                     // apply
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0 || driver->applyingFrequency_)
            return;

          driver->applyingFrequency_ = true; // защита от рекурсии

          SI47XX_TuneTo(v);

          // Автоматическое переключение режима на основе частоты
          SI47XX_MODE newMode = SI47XX_FM;
          auto &modParam = driver->getParam(ParamId::Modulation);

          if (v < 3000000) {
            // < 30 MHz - AM или SSB
            uint32_t mod = modParam.get();
            if (mod == (uint32_t)ModType::USB)
              newMode = SI47XX_USB;
            else if (mod == (uint32_t)ModType::LSB)
              newMode = SI47XX_LSB;
            else
              newMode = SI47XX_AM;
          } else if (v < 7600000) {
            // 30-76 MHz - AM
            newMode = SI47XX_AM;
            modParam.set((uint32_t)ModType::AM);
          } else {
            // >= 76 MHz - FM broadcast
            newMode = SI47XX_FM;
            modParam.set((uint32_t)ModType::WFM);
          }

          if (driver->si4732mode_ != newMode) {
            SI47XX_SwitchMode(newMode);
            driver->si4732mode_ = newMode;
          }

          driver->applyingFrequency_ = false;
        },
        PARAM_PERSIST);

    // ====================================================================
    // MODULATION - зависит от частоты
    // ====================================================================
    params_[(uint8_t)ParamId::Modulation] = Param<uint32_t>(
        (uint32_t)ModType::FM, this, [](void *ctx) -> uint32_t { return 0; },
        [](void *ctx) -> uint32_t {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          uint32_t freq = driver->getParam(ParamId::Frequency).get();
          if (freq >= 7600000)
            return (uint32_t)ModType::WFM; // только FM
          if (freq >= 3000000)
            return (uint32_t)ModType::AM; // только AM
          return (uint32_t)ModType::LSB;  // AM/USB/LSB доступны
        },
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0 || driver->applyingFrequency_)
            return;

          uint32_t freq = driver->getParam(ParamId::Frequency).get();
          SI47XX_MODE newMode = SI47XX_FM;

          if (v == (uint32_t)ModType::WFM && freq >= 7600000) {
            newMode = SI47XX_FM;
          } else if (v == (uint32_t)ModType::AM && freq < 3000000) {
            newMode = SI47XX_AM;
          } else if (v == (uint32_t)ModType::USB && freq < 3000000) {
            newMode = SI47XX_USB;
          } else if (v == (uint32_t)ModType::LSB && freq < 3000000) {
            newMode = SI47XX_LSB;
          }

          if (driver->si4732mode_ != newMode) {
            SI47XX_SwitchMode(newMode);
            driver->si4732mode_ = newMode;
          }
        },
        PARAM_PERSIST);

    // ====================================================================
    // BANDWIDTH - зависит от режима (SSB или AM/FM)
    // ====================================================================
    params_[(uint8_t)ParamId::Bandwidth] = Param<uint32_t>(
        1, this, [](void *ctx) -> uint32_t { return 0; },
        [](void *ctx) -> uint32_t { return SI47XX_IsSSB() ? 5 : 6; },
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;

          if (SI47XX_IsSSB()) {
            SI47XX_SetSsbBandwidth((SI47XX_SsbFilterBW)v);
          } else {
            SI47XX_SetBandwidth((SI47XX_FilterBW)v, v > 3);
          }
        },
        PARAM_PERSIST);

    // ====================================================================
    // GAIN (AGC Index)
    // ====================================================================
    params_[(uint8_t)ParamId::Gain] = Param<uint32_t>(
        0, 0, 37, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;
          SI47XX_SetAutomaticGainControl(v > 0 ? 1 : 0, v);
        },
        PARAM_PERSIST);

    // ====================================================================
    // VOLUME
    // ====================================================================
    params_[(uint8_t)ParamId::Volume] = Param<uint32_t>(
        32, 0, 63, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;
          SI47XX_SetVolume(v);
        },
        PARAM_PERSIST);

    // ====================================================================
    // SQUELCH - зависит от режима (FM или AM)
    // ====================================================================
    params_[(uint8_t)ParamId::Squelch] = Param<uint32_t>(
        10, 0, 127, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;

          if (driver->si4732mode_ == SI47XX_FM) {
            SI47XX_SetSeekFmRssiThreshold(v);
          } else {
            SI47XX_SetSeekAmRssiThreshold(v);
          }
        },
        PARAM_PERSIST);

    // ====================================================================
    // STEP
    // ====================================================================
    params_[(uint8_t)ParamId::Step] =
        Param<uint32_t>(10000, 1000, 100000, nullptr, nullptr, PARAM_PERSIST);

    // ====================================================================
    // READ-ONLY PARAMETERS (измерения)
    // ====================================================================
    params_[(uint8_t)ParamId::RSSI] =
        Param<uint32_t>(0, 0, 127, PARAM_READONLY);
    params_[(uint8_t)ParamId::SNR] = Param<uint32_t>(0, 0, 127, PARAM_READONLY);
    params_[(uint8_t)ParamId::SquelchOpen] =
        Param<uint32_t>(0, 0, 1, PARAM_READONLY);

    // ====================================================================
    // STATE
    // ====================================================================
    params_[(uint8_t)ParamId::RxMode] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (v && driver->powerState_ == 0) {
            driver->getParam(ParamId::PowerOn).set(1);
          }
        },
        0);

    params_[(uint8_t)ParamId::Mute] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;

          uint32_t vol = v ? 0 : driver->getParam(ParamId::Volume).get();
          SI47XX_SetVolume(vol);
        },
        0);

    // ====================================================================
    // ACTIONS
    // ====================================================================
    params_[(uint8_t)ParamId::PowerOn] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (v && driver->powerState_ == 0) {
            driver->powerOn();
          }
        },
        PARAM_ACTION);

    params_[(uint8_t)ParamId::PowerOff] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<SI4732Driver *>(ctx);
          if (v && driver->powerState_ > 0) {
            driver->powerOff();
          }
        },
        PARAM_ACTION);
  }

  RadioType getRadioType() const override { return RadioType::SI4732; }
  const char *getRadioName() const override { return "SI4732"; }

  void powerOn() {
    if (powerState_ > 0)
      return;

    if (SI47XX_IsSSB()) {
      SI47XX_PatchPowerUp();
    } else {
      SI47XX_PowerUp();
    }

    powerState_ = 1;

    // Применяем все параметры после включения
    getParam(ParamId::Frequency).set(getParam(ParamId::Frequency).get(), true);
    getParam(ParamId::Volume).set(getParam(ParamId::Volume).get(), true);
    getParam(ParamId::Gain).set(getParam(ParamId::Gain).get(), true);
    getParam(ParamId::Squelch).set(getParam(ParamId::Squelch).get(), true);
  }

  void powerOff() {
    if (powerState_ == 0)
      return;
    SI47XX_PowerDown();
    powerState_ = 0;
  }

  // Обновление read-only параметров
  void updateMeasurements() {
    if (powerState_ == 0)
      return;

    RSQ_GET();
    getParam(ParamId::RSSI).set(rsqStatus.resp.RSSI, true);
    getParam(ParamId::SNR).set(rsqStatus.resp.SNR, true);
    getParam(ParamId::SquelchOpen).set(rsqStatus.resp.SMUTE ? 0 : 1, true);
  }

private:
  SI47XX_MODE si4732mode_;
  bool applyingFrequency_; // защита от циклических зависимостей
};
