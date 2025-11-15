#pragma once

#include "../helper/channels.h"
#include "BK1080Driver.hpp"
#include "BK4819Driver.hpp"
#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "SI4732Driver.hpp"
#include "uart.h"

// ============================================================================
// VFO PROXY
// ============================================================================
class VFOProxy {
public:
  VFOProxy(IRadioDriver *driver) : driver_(driver) {}

  // ИСПРАВЛЕНИЕ: Добавлены обе версии operator[] (const и non-const)
  Param<uint32_t> &operator[](ParamId id) {
    if (driver_) {
      return (*driver_)[id];
    }
    static Param<uint32_t> dummy;
    return dummy;
  }

  const Param<uint32_t> &operator[](ParamId id) const {
    if (driver_) {
      return (*driver_)[id];
    }
    static Param<uint32_t> dummy;
    return dummy;
  }

  IRadioDriver *operator->() { return driver_; }
  const IRadioDriver *operator->() const { return driver_; }
  operator bool() const { return driver_ != nullptr; }

private:
  IRadioDriver *driver_;
};

// ============================================================================
// VFO BANK
// ============================================================================
class VFOBank {
public:
  VFOBank() : activeVFO_(0), scanningVFO_(0xFFFF) {}

  // === VFO MANAGEMENT ===

  VFOProxy getActiveVFO() { return VFOProxy(radios_[activeVFO_]); }

  VFOProxy getVFO(uint8_t index) {
    if (index < 3) {
      return VFOProxy(radios_[index]);
    }
    return VFOProxy(nullptr);
  }

  void setActiveVFO(uint8_t index) {
    if (index < 3) {
      activeVFO_ = index;
    }
  }

  uint8_t getActiveVFOIndex() const { return activeVFO_; }

  void updateMeasurements() {
    auto vfo = getActiveVFO();
    switch (vfo->getRadioType()) {
    case RadioType::BK4819: {
      BK4819Driver *drv = static_cast<BK4819Driver *>(vfo.operator->());
      drv->updateMeasurements();
      break;
    }
    case RadioType::SI4732: {
      SI4732Driver *drv = static_cast<SI4732Driver *>(vfo.operator->());
      drv->updateMeasurements();
      break;
    }
    case RadioType::BK1080: {
      BK1080Driver *drv = static_cast<BK1080Driver *>(vfo.operator->());
      drv->updateMeasurements();
      break;
    }
    default:
      break;
    }
  }

  // === RADIO SETUP ===

  void setRadio(uint8_t vfoIndex, IRadioDriver *driver) {
    if (vfoIndex < 3) {
      radios_[vfoIndex] = driver;
    }
  }

  IRadioDriver *getRadio(uint8_t vfoIndex) {
    return (vfoIndex < 3) ? radios_[vfoIndex] : nullptr;
  }

  // === CHANNEL OPERATIONS ===

  void loadChannel(uint8_t vfoIndex, uint16_t chNum) {
    auto vfo = getVFO(vfoIndex);
    if (vfo) {
      vfo->loadCh(chNum);
    }
  }

  void saveChannel(uint8_t vfoIndex) {
    auto vfo = getVFO(vfoIndex);
    if (vfo) {
      vfo->saveCh();
    }
  }

  bool loadChannelAuto(uint8_t vfoIndex, uint16_t chNum) {
    if (vfoIndex >= 3)
      return false;

    // 1. Прочитать канал один раз
    MR ch;
    CHANNELS_Load(chNum, &ch);

    // 2. Определить тип радио из канала
    RadioType radioType = (RadioType)ch.radio;

    // 3. Найти или создать нужный драйвер
    IRadioDriver *driver = getRadioForType(radioType);
    if (!driver)
      return false;

    // 4. Установить драйвер в VFO
    radios_[vfoIndex] = driver;

    // 5. Загрузить канал в драйвер
    driver->loadCh(chNum);

    return true;
  }

  void saveActiveChannel() { saveChannel(activeVFO_); }

  // === CONVENIENCE ACCESSORS ===

  // Упрощённый доступ к параметрам активного VFO
  Param<uint32_t> &operator[](ParamId id) { return getActiveVFO()[id]; }

  // Транзакции для батчинга изменений
  void beginTransaction() {
    auto vfo = getActiveVFO();
    if (vfo) {
      vfo->beginTransaction();
    }
  }

  void endTransaction() {
    auto vfo = getActiveVFO();
    if (vfo) {
      vfo->endTransaction();
    }
  }

  // === SCANNING ===

  void startScan(uint8_t vfoIndex) { scanningVFO_ = vfoIndex; }

  void stopScan() { scanningVFO_ = 0xFFFF; }

  bool isScanning() const { return scanningVFO_ != 0xFFFF; }

  uint8_t getScanningVFO() const {
    return (scanningVFO_ < 3) ? scanningVFO_ : 0xFF;
  }

  // === DEBUG ===

  void dumpState() {
    auto vfo = getActiveVFO();
    if (!vfo)
      return;

    Log("=== VFO %u: %s ===", activeVFO_, vfo->getRadioName());
    Log("Freq: %lu Hz", vfo[ParamId::Frequency].get());
    Log("Mod: %u", vfo[ParamId::Modulation].get());
    Log("BW: %u", vfo[ParamId::Bandwidth].get());
    Log("Gain: %u", vfo[ParamId::Gain].get());
    Log("Squelch: %u", vfo[ParamId::Squelch].get());
    Log("Step: %lu", vfo[ParamId::Step].get());
    Log("Volume: %u", vfo[ParamId::Volume].get());
  }

private:
  IRadioDriver *radios_[3] = {nullptr, nullptr, nullptr};
  uint8_t activeVFO_;
  uint16_t scanningVFO_;

  IRadioDriver *getRadioForType(RadioType type) {
    static BK4819Driver bk4819;
    static SI4732Driver si4732;
    static BK1080Driver bk1080;

    switch (type) {
    case RadioType::BK4819:
      return &bk4819;
    case RadioType::SI4732:
      return &si4732;
    case RadioType::BK1080:
      return &bk1080;
    default:
      return nullptr;
    }
  }
};
