#pragma once

#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "si473x.h"

// ============================================================================
// SI47XX HARDWARE (расширенный)
// ============================================================================

class SI4732Driver : public IRadioDriver {
public:
  SI4732Driver() : powerState_(RadioPowerState::OFF) {}
  RadioType getRadioType() const override { return RadioType::SI4732; }

  // ========================================================================
  // POWER MANAGEMENT
  // ========================================================================

  void powerOn() override {
    if (powerState_ == RadioPowerState::OFF) {
      // SI47XX_PowerUp();
      powerState_ = RadioPowerState::STANDBY;
    }
  }

  void powerOff() override {
    if (powerState_ != RadioPowerState::OFF) {
      // SI47XX_PowerDown();
      powerState_ = RadioPowerState::OFF;
    }
  }

  void setRxMode() override {
    if (powerState_ == RadioPowerState::OFF) {
      powerOn();
    }
    // SI47XX_TuneFrequency();
    powerState_ = RadioPowerState::RX;
  }

  void setStandby() override {
    if (powerState_ == RadioPowerState::RX) {
      // SI47XX_Standby();
      powerState_ = RadioPowerState::STANDBY;
    }
  }

  RadioPowerState getPowerState() const override { return powerState_; }
  bool isRxActive() const override {
    return powerState_ == RadioPowerState::RX;
  }

  // ========================================================================
  // SQUELCH (SI47XX использует SNR вместо squelch)
  // ========================================================================

  bool isSquelchOpen() override {
    if (powerState_ != RadioPowerState::RX)
      return false;

    // SI47XX использует SNR для определения сигнала
    uint8_t snr = readSNR();
    return snr > snrThreshold_;
  }

  void setSquelchThreshold(uint8_t threshold) { snrThreshold_ = threshold; }

  uint8_t readSNR() const override {
    if (powerState_ != RadioPowerState::RX)
      return 0;
    // return SI47XX_GetSNR();
    return 0;
  }

  // ========================================================================
  // RADIO PARAMETERS
  // ========================================================================

  void setFrequency(uint32_t freq) override {
    if (powerState_ == RadioPowerState::OFF)
      return;
    // SI47XX_SetFrequency(freq);
  }

  void setGain(uint8_t gain) override {
    if (powerState_ == RadioPowerState::OFF)
      return;
    // SI47XX_SetGain(gain);
  }

  void setBandwidth(uint16_t bw) override {
    if (powerState_ == RadioPowerState::OFF)
      return;
    // SI47XX_SetBandwidth(bw);
  }

  void setModulation(ModulationType mod) override {
    if (powerState_ == RadioPowerState::OFF)
      return;
    // SI47XX_SetMode(mod);
  }

  uint16_t readRSSI() const override {
    if (powerState_ != RadioPowerState::RX)
      return 0;
    // return SI47XX_GetRSSI();
    return 0;
  }

  const char *getRadioName() const override { return "SI4732"; }

  uint32_t getFrequency() const override { return state_.frequency; }

  ModulationType getModulation() const override { return state_.modulation; }

  uint8_t getGain() const override { return state_.gain; }

  uint16_t getBandwidth() const override { return state_.bandwidth; }

  // ========================================================================
  // AUDIO
  // ========================================================================

  void setVolume(uint8_t gain) override { SI47XX_SetVolume(gain); }

  void muteAudio(bool mute) override {
    audioMuted_ = mute;
    // SI47XX_SetMute(mute);
  }

  bool isAudioMuted() const override { return audioMuted_; }

  // ========================================================================
  // BOUNDARIES
  // ========================================================================

  uint32_t getMinFrequency() const override { return SI47XX_F_MIN; }
  uint32_t getMaxFrequency() const override { return SI47XX_F_MAX; }
  uint8_t getMaxGain() const override { return 30; }
  uint32_t getMaxBandwidth() const override { return 3; }

private:
  RadioPowerState powerState_;
  uint8_t snrThreshold_ = 20;
  bool audioMuted_ = false;
};
