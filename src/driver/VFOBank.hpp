#pragma once

#include "../helper/channels.h"
#include "BK1080Driver.hpp"
#include "BK4819Driver.hpp"
#include "IRadioDriver.hpp"
#include "ParamFormat.hpp"
#include "RadioCommon.hpp"
#include "SI4732Driver.hpp"
#include "uart.h"

// ============================================================================
// VFO STATE - хранит параметры для одного VFO
// ============================================================================

struct VFOState {
  uint32_t params[(uint8_t)ParamId::Count];
  uint16_t channelIndex;
  RadioType radioType;

  VFOState() : channelIndex(0xFFFF), radioType(RadioType::BK4819) {
    params[(uint8_t)ParamId::Frequency] = 145500000;
    params[(uint8_t)ParamId::Modulation] = 0;
    params[(uint8_t)ParamId::Bandwidth] = 2;
    params[(uint8_t)ParamId::Gain] = 34;
    params[(uint8_t)ParamId::Squelch] = 3;
    params[(uint8_t)ParamId::Step] = 2500;
    params[(uint8_t)ParamId::Volume] = 8;
    params[(uint8_t)ParamId::PowerState] = 0;
    params[(uint8_t)ParamId::RxMode] = 0;
    params[(uint8_t)ParamId::Mute] = 0;

    for (uint8_t i = (uint8_t)ParamId::RSSI; i <= (uint8_t)ParamId::SquelchOpen;
         ++i) {
      params[i] = 0;
    }
  }
};

// Forward declaration
class VFOBank;

// ============================================================================
// PARAM PROXY - для operator[] с поддержкой +=, -=, =
// ============================================================================

class ParamProxy {
public:
  ParamProxy(VFOBank *bank, ParamId id) : bank_(bank), id_(id) {}

  // ОБЪЯВЛЕНИЯ (реализация ПОСЛЕ VFOBank)
  uint32_t get() const;
  const char *toString(char *buf, size_t bufSize) const;
  uint32_t getRealValue() const;

  // Операторы присваивания
  ParamProxy &operator=(uint32_t value);
  ParamProxy &operator+=(uint32_t value);
  ParamProxy &operator-=(uint32_t value);

  // Неявное преобразование в uint32_t
  operator uint32_t() const { return get(); }

private:
  VFOBank *bank_;
  ParamId id_;

  // Friend для доступа к VFOBank
  friend class VFOBank;
};

// ============================================================================
// VFO BANK С РАЗДЕЛЬНЫМ ХРАНЕНИЕМ ПАРАМЕТРОВ
// ============================================================================

class VFOBank {
public:
  VFOBank() : activeVFO_(0), scanningVFO_(0xFFFF) {
    bk4819_ = nullptr;
    si4732_ = nullptr;
    bk1080_ = nullptr;
  }

  // === OPERATOR[] ДЛЯ УДОБНОГО ДОСТУПА ===

  ParamProxy operator[](ParamId id) { return ParamProxy(this, id); }

  // === VFO MANAGEMENT ===

  void setActiveVFO(uint8_t index) {
    if (index < 3) {
      activeVFO_ = index;
    }
  }

  uint8_t getActiveVFOIndex() const { return activeVFO_; }

  RadioType getRadioType() const { return states_[activeVFO_].radioType; }

  // === УМНОЕ ПЕРЕКЛЮЧЕНИЕ VFO ===

  void switchVFO(uint8_t newVFO) {
    if (newVFO >= 3 || newVFO == activeVFO_)
      return;

    saveDriverStateToVFO(activeVFO_);
    states_[activeVFO_].params[(uint8_t)ParamId::RxMode] = 0;

    activeVFO_ = newVFO;

    loadVFOStateToDriver(newVFO);

    if (states_[newVFO].params[(uint8_t)ParamId::PowerState] == 0) {
      applyParam(ParamId::PowerOn, 1);
    }
    applyParam(ParamId::RxMode, 1);
  }

  // === CHANNEL OPERATIONS ===

  bool loadChannelAuto(uint8_t vfoIndex, uint16_t chNum) {
    if (vfoIndex >= 3 || chNum == 0xFFFF)
      return false;

    MR ch;
    CHANNELS_Load(chNum, &ch);

    RadioType radioType = (RadioType)ch.radio;
    states_[vfoIndex].radioType = radioType;
    states_[vfoIndex].channelIndex = chNum;

    states_[vfoIndex].params[(uint8_t)ParamId::Frequency] = ch.rxF;
    states_[vfoIndex].params[(uint8_t)ParamId::Modulation] = ch.modulation;
    states_[vfoIndex].params[(uint8_t)ParamId::Bandwidth] = ch.bw;
    states_[vfoIndex].params[(uint8_t)ParamId::Gain] = ch.gainIndex;
    states_[vfoIndex].params[(uint8_t)ParamId::Squelch] = ch.squelch.value;
    states_[vfoIndex].params[(uint8_t)ParamId::Step] = ch.step;

    if (vfoIndex == activeVFO_) {
      loadVFOStateToDriver(vfoIndex);
    }

    return true;
  }

  bool loadActiveChannelAuto(uint16_t chNum) {
    return loadChannelAuto(activeVFO_, chNum);
  }

  void saveChannel(uint8_t vfoIndex) {
    if (vfoIndex >= 3)
      return;

    uint16_t chNum = states_[vfoIndex].channelIndex;
    if (chNum == 0xFFFF)
      return;

    if (vfoIndex == activeVFO_) {
      saveDriverStateToVFO(vfoIndex);
    }

    MR ch;
    CHANNELS_Load(chNum, &ch);

    ch.rxF = states_[vfoIndex].params[(uint8_t)ParamId::Frequency];
    ch.modulation = states_[vfoIndex].params[(uint8_t)ParamId::Modulation];
    ch.bw = states_[vfoIndex].params[(uint8_t)ParamId::Bandwidth];
    ch.gainIndex = states_[vfoIndex].params[(uint8_t)ParamId::Gain];
    ch.squelch.value = states_[vfoIndex].params[(uint8_t)ParamId::Squelch];
    ch.step = states_[vfoIndex].params[(uint8_t)ParamId::Step];
    ch.radio = (uint8_t)states_[vfoIndex].radioType;

    CHANNELS_Save(chNum, &ch);
  }

  void saveActiveChannel() { saveChannel(activeVFO_); }

  void loadVfos() {
    uint8_t vfoIdx = 0;
    for (uint16_t i = 0; i < CHANNELS_GetCountMax(); ++i) {
      CHMeta meta = CHANNELS_GetMeta(i);

      bool isOurType = (TYPE_FILTER_VFO & (1 << meta.type)) != 0;
      if (!isOurType) {
        continue;
      }

      loadChannelAuto(vfoIdx, i);
      vfoIdx++;
    }
  }

  // === ВНУТРЕННИЕ МЕТОДЫ ДЛЯ ParamProxy ===
  uint32_t get(ParamId id) {
    if (id == ParamId::Radio)
      return (uint32_t)states_[activeVFO_].radioType;
    return states_[activeVFO_].params[(uint8_t)id];
  }

  void set(ParamId id, uint32_t value) {
    if (id == ParamId::Radio) {
      states_[activeVFO_].radioType = (RadioType)value;
      return;
    }
    states_[activeVFO_].params[(uint8_t)id] = value;
    applyParam(id, value);
  }

  void add(ParamId id, uint32_t value) {
    uint32_t current = states_[activeVFO_].params[(uint8_t)id];

    IRadioDriver *driver = getDriverForVFO(activeVFO_);
    if (driver) {
      uint32_t maxVal = driver->getParam(id).getMax();
      if (current <= maxVal - value) {
        set(id, current + value);
      } else {
        set(id, maxVal);
      }
    } else {
      set(id, current + value);
    }
  }

  void subtract(ParamId id, uint32_t value) {
    uint32_t current = states_[activeVFO_].params[(uint8_t)id];

    IRadioDriver *driver = getDriverForVFO(activeVFO_);
    if (driver) {
      uint32_t minVal = driver->getParam(id).getMin();
      if (current >= minVal + value) {
        set(id, current - value);
      } else {
        set(id, minVal);
      }
    } else {
      if (current >= value) {
        set(id, current - value);
      } else {
        set(id, 0);
      }
    }
  }

  // === UPDATE MEASUREMENTS ===

  void updateMeasurements() {
    IRadioDriver *driver = getDriverForVFO(activeVFO_);
    if (!driver)
      return;

    switch (driver->getRadioType()) {
    case RadioType::BK4819: {
      BK4819Driver *drv = static_cast<BK4819Driver *>(driver);
      drv->updateMeasurements();
      break;
    }
    case RadioType::SI4732: {
      SI4732Driver *drv = static_cast<SI4732Driver *>(driver);
      drv->updateMeasurements();
      break;
    }
    case RadioType::BK1080: {
      BK1080Driver *drv = static_cast<BK1080Driver *>(driver);
      drv->updateMeasurements();
      break;
    }
    default:
      break;
    }

    states_[activeVFO_].params[(uint8_t)ParamId::RSSI] =
        driver->getParam(ParamId::RSSI).get();
    states_[activeVFO_].params[(uint8_t)ParamId::Noise] =
        driver->getParam(ParamId::Noise).get();
    states_[activeVFO_].params[(uint8_t)ParamId::Glitch] =
        driver->getParam(ParamId::Glitch).get();
    states_[activeVFO_].params[(uint8_t)ParamId::SNR] =
        driver->getParam(ParamId::SNR).get();
    states_[activeVFO_].params[(uint8_t)ParamId::SquelchOpen] =
        driver->getParam(ParamId::SquelchOpen).get();
  }

  // === HELPER ФУНКЦИИ ===

  void powerOnAndReceive() {
    set(ParamId::PowerOn, 1);
    set(ParamId::RxMode, 1);
  }

  // === DEBUG ===

  void dumpState() {
    VFOState &state = states_[activeVFO_];
    IRadioDriver *driver = getDriverForVFO(activeVFO_);

    Log("=== VFO %u ===", activeVFO_);
    if (driver) {
      Log("Radio: %s", driver->getRadioName());
    }
    Log("Freq: %lu Hz", state.params[(uint8_t)ParamId::Frequency]);
    Log("Mod: %u", state.params[(uint8_t)ParamId::Modulation]);
    Log("BW: %u", state.params[(uint8_t)ParamId::Bandwidth]);
    Log("Gain: %u", state.params[(uint8_t)ParamId::Gain]);
    Log("Squelch: %u", state.params[(uint8_t)ParamId::Squelch]);
    Log("Step: %lu", state.params[(uint8_t)ParamId::Step]);
    Log("Volume: %u", state.params[(uint8_t)ParamId::Volume]);
  }

private:
  VFOState states_[3];
  uint8_t activeVFO_;
  uint16_t scanningVFO_;

  BK4819Driver *bk4819_;
  SI4732Driver *si4732_;
  BK1080Driver *bk1080_;

  IRadioDriver *getDriverForVFO(uint8_t vfo) {
    RadioType type = states_[vfo].radioType;

    switch (type) {
    case RadioType::BK4819:
      if (!bk4819_)
        bk4819_ = new BK4819Driver();
      return bk4819_;
    case RadioType::SI4732:
      if (!si4732_)
        si4732_ = new SI4732Driver();
      return si4732_;
    case RadioType::BK1080:
      if (!bk1080_)
        bk1080_ = new BK1080Driver();
      return bk1080_;
    default:
      return nullptr;
    }
  }

  void saveDriverStateToVFO(uint8_t vfo) {
    IRadioDriver *driver = getDriverForVFO(vfo);
    if (!driver)
      return;

    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      states_[vfo].params[i] = driver->getParam((ParamId)i).get();
    }
  }

  void loadVFOStateToDriver(uint8_t vfo) {
    IRadioDriver *driver = getDriverForVFO(vfo);
    if (!driver)
      return;

    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      driver->getParam((ParamId)i).set(states_[vfo].params[i], true);
    }
  }

  void applyParam(ParamId id, uint32_t value) {
    IRadioDriver *driver = getDriverForVFO(activeVFO_);
    if (driver) {
      driver->getParam(id).set(value, true);
    }
  }

  friend class ParamProxy;
};

inline uint32_t ParamProxy::get() const { return bank_->get(id_); }

inline const char *ParamProxy::toString(char *buf, size_t bufSize) const {
  if (!bank_) {
    snprintf(buf, bufSize, "N/A");
    return buf;
  }

  uint32_t value = bank_->get(id_);

  switch (id_) {
  case ParamId::Radio:
    snprintf(buf, bufSize, "%s",
             ParamFormat::getRadioTypeName((RadioType)value));
    break;

  case ParamId::Modulation:
    snprintf(buf, bufSize, "%s", ParamFormat::getModulationName(value));
    break;

  case ParamId::Bandwidth: {
    RadioType radioType = bank_->getRadioType();
    snprintf(buf, bufSize, "%s",
             ParamFormat::getBandwidthName(
                 radioType, bank_->get(ParamId::Modulation), value));
    break;
  }

  case ParamId::Frequency:
    snprintf(buf, bufSize, "%lu", value);
    break;

  default:
    snprintf(buf, bufSize, "%u", value);
    break;
  }

  return buf;
}

inline uint32_t ParamProxy::getRealValue() const {
  if (!bank_)
    return 0;

  uint32_t value = bank_->get(id_);

  switch (id_) {
  case ParamId::Bandwidth: {
    RadioType radioType = bank_->getRadioType();
    // Добавь логику преобразования
    return value;
  }

  default:
    return value;
  }
}

inline ParamProxy &ParamProxy::operator=(uint32_t value) {
  bank_->set(id_, value);
  return *this;
}

inline ParamProxy &ParamProxy::operator+=(uint32_t value) {
  bank_->add(id_, value);
  return *this;
}

inline ParamProxy &ParamProxy::operator-=(uint32_t value) {
  bank_->subtract(id_, value);
  return *this;
}
