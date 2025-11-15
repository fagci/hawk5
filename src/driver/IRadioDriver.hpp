#pragma once
#include "../helper/channels.h"
#include "RadioCommon.hpp"
#include <cstring>

// ============================================================================
// RADIO DRIVER INTERFACE
// ============================================================================

#pragma once

#include "../helper/channels.h"
#include "RadioCommon.hpp"

using ParamProxyFunc = ParamProxy<std::function<void(uint32_t)>>;

class IRadioDriver {
public:
  IRadioDriver() : channelIndex_(0xFFFF), powerState_(0) {
    // Инициализация пустыми параметрами
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      params_[i] = Param<std::function<void(uint32_t)>>();
    }
  }

  virtual ~IRadioDriver() = default;

  virtual RadioType getRadioType() const = 0;
  virtual const char *getRadioName() const = 0;

  virtual ParamProxyFunc operator[](ParamId id) {
    return params_[(uint8_t)id].proxy();
  }

  virtual bool isDirty() const {
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      if (params_[i].isDirty() && (params_[i].flags() & PARAM_PERSIST)) {
        return true;
      }
    }
    return false;
  }

  virtual void applyToHardware() {
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      // Применяем только writable параметры с настройками
      if ((params_[i].flags() & PARAM_WRITABLE) &&
          !(params_[i].flags() & PARAM_ACTION) &&
          params_[i].flags() & PARAM_PERSIST) {
        // Триггерим apply через set с текущим значением
        uint32_t val = params_[i].get();
        params_[i].set(val);
      }
    }
  }

  /* virtual void loadCh(uint16_t channelIndex, const MR *ch) = 0;
  virtual void saveCh() = 0; */
  void loadCh(uint16_t channelIndex, const MR *ch) {
    channelIndex_ = channelIndex;
    params_[(uint8_t)ParamId::Frequency].setQuiet(ch->rxF);
    params_[(uint8_t)ParamId::Gain].setQuiet(ch->gainIndex);
    params_[(uint8_t)ParamId::Bandwidth].setQuiet(ch->bw);
    params_[(uint8_t)ParamId::Modulation].setQuiet(ch->modulation);
    params_[(uint8_t)ParamId::Squelch].setQuiet(ch->squelch.value);
    clearDirty();
  }

  void saveCh() {
    if (!isDirty())
      return;
    MR ch;
    CHANNELS_Load(channelIndex_, &ch);
    ch.rxF = params_[(uint8_t)ParamId::Frequency].get();
    ch.gainIndex = params_[(uint8_t)ParamId::Gain].get();
    ch.bw = params_[(uint8_t)ParamId::Bandwidth].get();
    ch.modulation = params_[(uint8_t)ParamId::Modulation].get();
    ch.squelch.value = params_[(uint8_t)ParamId::Squelch].get();
    CHANNELS_Save(channelIndex_, &ch);
    clearDirty();
  }

protected:
  void clearDirty() {
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      params_[i].clearDirty();
    }
  }

  uint16_t channelIndex_;
  uint32_t powerState_; // 0=OFF, 1=RX/STANDBY, 2=TX
  Param<std::function<void(uint32_t)>> params_[(uint8_t)ParamId::Count];
};
