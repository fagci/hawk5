#pragma once
#include "IRadioDriver.hpp"
#include "bk1080.h"

class BK1080Driver : public IRadioDriver {
public:
  BK1080Driver() : powerState_(RadioPowerState::OFF) {}
  RadioType getRadioType() const override { return RadioType::BK1080; }
  const char *getRadioName() const override { return "BK1080"; }

  void powerOn() override {
    if (powerState_ == RadioPowerState::OFF) {
      BK1080_Init(0, true);
      powerState_ = RadioPowerState::STANDBY;
    }
  }

  void powerOff() override {
    if (powerState_ != RadioPowerState::OFF) {
      BK1080_Init(0, false);
      powerState_ = RadioPowerState::OFF;
    }
  }

  void setRxMode() override {
    if (powerState_ == RadioPowerState::OFF)
      powerOn();
    BK1080_Init(0, true);
    powerState_ = RadioPowerState::RX;
  }

  void setStandby() override {
    if (powerState_ == RadioPowerState::RX) {
      BK1080_Init(0, false);
      powerState_ = RadioPowerState::STANDBY;
    }
  }

  RadioPowerState getPowerState() const override { return powerState_; }
  bool isRxActive() const override {
    return powerState_ == RadioPowerState::RX;
  }

  void setFrequency(uint32_t freq) override {
    if (powerState_ == RadioPowerState::OFF)
      return;
    currentFreq_ = freq;
    BK1080_SetFrequency(freq / 1000); // Convert Hz to kHz
  }

  uint32_t getFrequency() const override { return currentFreq_; }
  uint32_t getMinFrequency() const override { return 87000000; }
  uint32_t getMaxFrequency() const override { return 108000000; }

  void setModulation(ModulationType mod) override {
    currentModulation_ = MOD_WFM;
  }
  ModulationType getModulation() const override { return MOD_WFM; }

  void setGain(uint8_t gain) override {
    gain_ = gain;
    // BK1080_SetVolume(gain);
  }

  uint8_t getGain() const override { return gain_; }
  uint8_t getMaxGain() const override { return 15; }

  void setBandwidth(uint16_t bw) override { bandwidth_ = bw; }
  uint16_t getBandwidth() const override { return bandwidth_; }

  void setSquelch(uint8_t threshold) override { rssiThreshold_ = threshold; }
  uint8_t getSquelch() const override { return rssiThreshold_; }

  bool isSquelchOpen() override {
    if (powerState_ != RadioPowerState::RX)
      return false;
    return readRSSI() > rssiThreshold_;
  }

  uint16_t readRSSI() const override {
    return (powerState_ == RadioPowerState::RX) ? BK1080_GetRSSI() : 0;
  }

  uint8_t readSNR() const override {
    return (powerState_ == RadioPowerState::RX) ? BK1080_GetSNR() : 0;
  }

  void setVolume(uint8_t volume) override {
    // BK1080_SetVolume(volume);
  }

  void muteAudio(bool mute) override {
    audioMuted_ = mute;
    BK1080_Mute(mute);
  }

  bool isAudioMuted() const override { return audioMuted_; }

private:
  RadioPowerState powerState_;
  uint32_t currentFreq_ = 90000000;
  ModulationType currentModulation_ = MOD_WFM;
  uint8_t gain_ = 10;
  uint16_t bandwidth_ = 0;
  uint8_t rssiThreshold_ = 100;
  bool audioMuted_ = false;
};
