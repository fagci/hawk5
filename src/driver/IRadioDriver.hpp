#pragma once
#include "../helper/channels.h"
#include "RadioCommon.hpp"
#include <cstring>

// ============================================================================
// RADIO DRIVER INTERFACE
// ============================================================================

class IRadioDriver {
public:
  virtual ~IRadioDriver() = default;
  virtual RadioType getRadioType() const = 0;
  virtual const char *getRadioName() const = 0;

  // ========================================================================
  // POWER MANAGEMENT
  // ========================================================================
  virtual void powerOn() = 0;
  virtual void powerOff() = 0;
  virtual void setRxMode() = 0;
  virtual void setStandby() = 0;
  virtual RadioPowerState getPowerState() const = 0;
  virtual bool isRxActive() const = 0;

  // ========================================================================
  // FREQUENCY
  // ========================================================================
  virtual void setFrequency(uint32_t freq) = 0;
  virtual uint32_t getFrequency() const = 0;
  virtual uint32_t getMinFrequency() const = 0;
  virtual uint32_t getMaxFrequency() const = 0;

  // ========================================================================
  // MODULATION
  // ========================================================================
  virtual void setModulation(ModulationType mod) = 0;
  virtual ModulationType getModulation() const = 0;

  // ========================================================================
  // GAIN & BANDWIDTH
  // ========================================================================
  virtual void setGain(uint8_t gain) = 0;
  virtual uint8_t getGain() const = 0;
  virtual uint8_t getMaxGain() const = 0;

  virtual void setBandwidth(uint16_t bw) = 0;
  virtual uint16_t getBandwidth() const = 0;
  virtual uint32_t getMaxBandwidth() const { return 0; }

  // ========================================================================
  // SQUELCH
  // ========================================================================
  virtual bool isSquelchOpen() = 0;

  // ========================================================================
  // MEASUREMENTS
  // ========================================================================
  virtual uint16_t readRSSI() const = 0;
  virtual uint8_t readNoise() const { return 0; }
  virtual uint8_t readGlitch() const { return 0; }
  virtual uint8_t readSNR() const { return 0; }

  virtual uint16_t readVoiceAmplitude() const { return 0; }

  virtual void setSquelch(uint8_t level) {
    // Default: ничего не делаем
    (void)level;
  }

  virtual uint8_t getSquelch() const { return 0; }

  // ========================================================================
  // AUDIO
  // ========================================================================
  virtual void setVolume(uint8_t volume) = 0;
  virtual void muteAudio(bool mute) = 0;
  virtual bool isAudioMuted() const = 0;

  // ========================================================================
  // TRANSMIT (optional - not all radios support TX)
  // ========================================================================
  virtual bool supportsTX() const { return false; }
  virtual void startTX(uint32_t txFreq, uint8_t powerLevel, bool paEnabled) {}
  virtual void stopTX() {}

  bool isDirty() { return state_.dirty; }

  void loadCh(uint16_t channelIndex, const MR *ch) {

    // Загружаем все параметры из канала
    channelIndex_ = channelIndex;
    state_.frequency = ch->rxF;
    state_.gain = ch->gainIndex;
    state_.bandwidth = ch->bw;
    state_.modulation = (ModulationType)ch->modulation;
    state_.squelch = ch->squelch.value;

    // Копируем имя канала
    memcpy(state_.name, ch->name, sizeof(state_.name));

    state_.enabled = true;
    state_.dirty = false;
  }

  void saveCh() {
    if (!state_.dirty)
      return;

    MR ch;
    CHANNELS_Load(channelIndex_, &ch);

    // Сохраняем изменённые параметры
    ch.rxF = state_.frequency;
    ch.gainIndex = state_.gain;
    ch.bw = state_.bandwidth;
    ch.modulation = state_.modulation;
    ch.squelch.value = state_.squelch;
    memcpy(ch.name, state_.name, sizeof(ch.name));

    CHANNELS_Save(channelIndex_, &ch);
    state_.dirty = false;
  }

  uint16_t getChannelIndex() const { return channelIndex_; }

protected:
  RadioPowerState powerState_;
  VFOState state_;
  uint16_t channelIndex_ = 0xFFFF; // Индекс канала в памяти
};
