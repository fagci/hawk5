#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "bk4819.h"
#include <cstring>

class BK4819Driver : public IRadioDriver {
public:
  BK4819Driver() {}
  RadioType getRadioType() const override { return RadioType::BK4819; }
  const char *getRadioName() const override { return "BK4819"; }

  // ========================================================================
  // POWER MANAGEMENT
  // ========================================================================

  void powerOn() override {
    if (powerState_ == RadioPowerState::OFF) {
      BK4819_Init();
      powerState_ = RadioPowerState::STANDBY;
    }
  }

  void powerOff() override {
    if (powerState_ != RadioPowerState::OFF) {
      BK4819_Idle();
      powerState_ = RadioPowerState::OFF;
    }
  }

  void setRxMode() override {
    if (powerState_ == RadioPowerState::OFF)
      powerOn();
    BK4819_RX_TurnOn();
    powerState_ = RadioPowerState::RX;
    applyToHardware(); // Применяем настройки VFO
  }

  void setStandby() override {
    if (powerState_ == RadioPowerState::RX ||
        powerState_ == RadioPowerState::TX) {
      BK4819_Idle();
      powerState_ = RadioPowerState::STANDBY;
    }
  }

  RadioPowerState getPowerState() const override { return powerState_; }
  bool isRxActive() const override {
    return powerState_ == RadioPowerState::RX;
  }

  // ========================================================================
  // FREQUENCY (с кешированием)
  // ========================================================================

  void setFrequency(uint32_t freq) override {
    if (freq >= getMinFrequency() && freq <= getMaxFrequency()) {
      state_.frequency = freq;
      state_.dirty = true;

      if (powerState_ != RadioPowerState::OFF) {
        BK4819_TuneTo(freq, false);
        BK4819_SelectFilterEx((freq < SETTINGS_GetFilterBound()) ? FILTER_VHF
                                                                 : FILTER_UHF);
      }
    }
  }

  // ========================================================================
  // MODULATION
  // ========================================================================

  void setModulation(ModulationType mod) override {
    state_.modulation = mod;
    state_.dirty = true;
    if (powerState_ != RadioPowerState::OFF) {
      BK4819_SetModulation(mod);
    }
  }

  ModulationType getModulation() const override { return state_.modulation; }

  // ========================================================================
  // GAIN & BANDWIDTH
  // ========================================================================

  void setGain(uint8_t gain) override {
    if (gain <= 31) { // BK4819 имеет 32 уровня усиления (0-31)
      state_.gain = gain;
      state_.dirty = true;
      if (powerState_ != RadioPowerState::OFF) {
        BK4819_SetAGC(state_.modulation != MOD_AM, gain);
      }
    }
  }

  uint8_t getGain() const override { return state_.gain; }

  uint8_t getMaxGain() const override {
    return 31; // BK4819 поддерживает 32 уровня (0-31)
  }

  void setBandwidth(uint16_t bw) override {
    if (bw <= BK4819_FILTER_BW_26k) {
      state_.bandwidth = bw;
      state_.dirty = true;
      if (powerState_ != RadioPowerState::OFF) {
        BK4819_SetFilterBandwidth((BK4819_FilterBandwidth_t)bw);
      }
    }
  }

  uint16_t getBandwidth() const override { return state_.bandwidth; }

  uint32_t getMaxBandwidth() const override {
    return BK4819_FILTER_BW_26k; // Максимальная полоса - 26kHz
  }

  // ========================================================================
  // SQUELCH
  // ========================================================================

  bool isSquelchOpen() override { return BK4819_IsSquelchOpen(); }

  // ========================================================================
  // AUDIO
  // ========================================================================

  void setVolume(uint8_t volume) override {
    // BK4819 использует DAC для регулировки громкости
    // Можно использовать регистр для управления громкостью
    // Пример: через регистр 0x47 (DAC Gain)
    if (volume <= 15) { // Обычно 0-15
      uint16_t regVal = BK4819_ReadRegister(BK4819_REG_47);
      regVal = (regVal & ~0x780) | ((volume & 0x0F) << 7);
      BK4819_WriteRegister(BK4819_REG_47, regVal);
    }
  }

  void muteAudio(bool mute) override {
    if (mute) {
      BK4819_SetAF(BK4819_AF_MUTE);
    } else {
      // Восстанавливаем AF в зависимости от модуляции
      switch (state_.modulation) {
      case MOD_FM:
        BK4819_SetAF(BK4819_AF_FM);
        break;
      case MOD_AM:
        BK4819_SetAF(BK4819_AF_AM);
        break;
      case MOD_USB:
      case MOD_LSB:
        BK4819_SetAF(BK4819_AF_USB);
        break;
      case MOD_BYP:
        BK4819_SetAF(BK4819_AF_BYPASS);
        break;
      case MOD_RAW:
        BK4819_SetAF(BK4819_AF_RAW);
        break;
      default:
        BK4819_SetAF(BK4819_AF_FM);
        break;
      }
    }
  }

  bool isAudioMuted() const override {
    // Проверяем регистр AF типа
    uint16_t regVal = BK4819_ReadRegister(BK4819_REG_47);
    return (regVal & 0x0F) == BK4819_AF_MUTE;
  }

  // ========================================================================
  // TRANSMIT (BK4819 поддерживает TX)
  // ========================================================================

  bool supportsTX() const override { return true; }

  void startTX(uint32_t txFreq, uint8_t powerLevel, bool paEnabled) override {
    if (powerState_ == RadioPowerState::OFF) {
      powerOn();
    }

    BK4819_PrepareTransmit();
    BK4819_TuneTo(txFreq, false);
    BK4819_SetupPowerAmplifier(powerLevel, txFreq);

    powerState_ = RadioPowerState::TX;
  }

  void stopTX() override {
    if (powerState_ == RadioPowerState::TX) {
      BK4819_ExitTxMute();
      setRxMode();
    }
  }

  uint32_t getFrequency() const override { return state_.frequency; }
  uint32_t getMinFrequency() const override { return BK4819_F_MIN; }
  uint32_t getMaxFrequency() const override { return BK4819_F_MAX; }

  uint16_t readRSSI() const override { return BK4819_GetRSSI(); }
  uint8_t readGlitch() const override { return BK4819_GetGlitch(); }
  uint8_t readNoise() const override { return BK4819_GetNoise(); }
  uint8_t readSNR() const override { return BK4819_GetSNR(); }

  uint16_t readVoiceAmplitude() const override {
    return BK4819_GetVoiceAmplitude();
  }

  void setSquelch(uint8_t level) override {
    state_.squelch = level;
    BK4819_Squelch(level, 100, 200);
  }

  uint8_t getSquelch() const override { return state_.squelch; }

  // ========================================================================
  // VFO-SPECIFIC
  // ========================================================================

  void applyToHardware() {
    if (powerState_ == RadioPowerState::OFF)
      return;

    BK4819_TuneTo(state_.frequency, false);
    BK4819_SetModulation(state_.modulation);
    BK4819_SetAGC(state_.modulation != MOD_AM, state_.gain);
    BK4819_SetFilterBandwidth(state_.bandwidth);
    BK4819_Squelch(state_.squelch, 100, 200);
  }

  const char *getName() const { return state_.name; }
  void setName(const char *name) {
    strncpy(state_.name, name, sizeof(state_.name) - 1);
    state_.name[sizeof(state_.name) - 1] = '\0';
    state_.dirty = true;
  }

  bool isEnabled() const { return state_.enabled; }
  void setEnabled(bool enabled) { state_.enabled = enabled; }
  bool isDirty() const { return state_.dirty; }
};
