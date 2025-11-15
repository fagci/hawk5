#pragma once
#include "../helper/channels.h"
#include "BK1080Driver.hpp"
#include "BK4819Driver.hpp"
#include "IRadioDriver.hpp"
#include "SI4732Driver.hpp"
#include "uart.h"

class VFOBank {
public:
  VFOBank() : activeVFO_(0), scanning_(false), scanIndex_(0) {
    vfoCount_ = 0;
    // Инициализируем массив драйверов
    for (uint8_t i = 0; i < MAX_VFO_COUNT; ++i) {
      vfos_[i] = nullptr;
    }
  }

  void loadAll() {
    vfoCount_ = 0;
    uint8_t vfoIdx = 0;
    for (uint16_t i = 0; i < CHANNELS_GetCountMax() && vfoIdx < MAX_VFO_COUNT;
         ++i) {
      CHMeta meta = CHANNELS_GetMeta(i);

      bool isOurType = (TYPE_FILTER_VFO & (1 << meta.type)) != 0;
      if (!isOurType) {
        continue;
      }

      MR ch;
      CHANNELS_Load(i, &ch);
      IRadioDriver *driver;
      if (ch.radio == RADIO_BK4819) {
        driver = &bk4819_;
      }
      if (ch.radio == RADIO_BK1080) {
        driver = &bk1080_;
      }
      if (ch.radio == RADIO_SI4732) {
        driver = &si4732_;
      }
      Log("DRIVER: %u", driver);
      vfos_[vfoIdx] = driver;

      LogC(LOG_C_BG_BLUE, "[VFOBank] CH %u -> VFO %u", i, vfoIdx);
      driver->loadCh(i, &ch);
      vfoIdx++;
    }
    vfoCount_ = vfoIdx;

    // Применяем параметры активного VFO к hardware
    if (vfos_[activeVFO_]) {
      applyVFOToHardware(activeVFO_);
    }
  }

  // ========================================================================
  // SAVE ALL VFOs (БЕЗ dynamic_cast!)
  // ========================================================================

  void saveAll() {
    for (uint8_t i = 0; i < MAX_VFO_COUNT; ++i) {
      if (!vfos_[i])
        continue;

      switch (vfos_[i]->getRadioType()) {
      case RadioType::BK4819: {
        auto *bk4819 = static_cast<BK4819Driver *>(vfos_[i]);
        if (bk4819->isDirty()) {
          bk4819->saveCh();
        }
        break;
      }
      case RadioType::SI4732: {
        auto *si4732 = static_cast<SI4732Driver *>(vfos_[i]);
        if (si4732->isDirty()) {
          si4732->saveCh();
        }
        break;
      }
      case RadioType::BK1080: {
        auto *bk1080 = static_cast<BK1080Driver *>(vfos_[i]);
        if (bk1080->isDirty()) {
          bk1080->saveCh();
        }
        break;
      }
      default:
        break;
      }
    }
  }

  // ========================================================================
  // APPLY VFO TO HARDWARE (вместо applyToHardware())
  // ========================================================================

  void applyVFOToHardware(uint8_t index) {
    IRadioDriver *driver = vfos_[index];
    if (!driver)
      return;

    switch (driver->getRadioType()) {
    case RadioType::BK4819: {
      auto *bk4819 = static_cast<BK4819Driver *>(driver);
      // Применяем все параметры к hardware
      bk4819->setFrequency(bk4819->getFrequency());
      bk4819->setModulation(bk4819->getModulation());
      bk4819->setGain(bk4819->getGain());
      bk4819->setBandwidth(bk4819->getBandwidth());
      bk4819->setSquelch(bk4819->getSquelch());
      break;
    }
    case RadioType::SI4732: {
      auto *si4732 = static_cast<SI4732Driver *>(driver);
      si4732->setFrequency(si4732->getFrequency());
      si4732->setModulation(si4732->getModulation());
      si4732->setGain(si4732->getGain());
      si4732->setBandwidth(si4732->getBandwidth());
      break;
    }
    case RadioType::BK1080: {
      auto *bk1080 = static_cast<BK1080Driver *>(driver);
      bk1080->setFrequency(bk1080->getFrequency());
      bk1080->setGain(bk1080->getGain());
      break;
    }
    default:
      break;
    }
  }

  // ========================================================================
  // SCAN TICK (БЕЗ dynamic_cast!)
  // ========================================================================

  void scanTick() {
    if (!scanning_)
      return;

    IRadioDriver *driver = vfos_[scanIndex_];
    if (!driver) {
      scanNext();
      return;
    }

    bool signalDetected = false;

    switch (driver->getRadioType()) {
    case RadioType::BK4819: {
      auto *bk4819 = static_cast<BK4819Driver *>(driver);
      uint16_t rssi = bk4819->readRSSI();
      signalDetected = (rssi > SCAN_RSSI_THRESHOLD);
      break;
    }
    case RadioType::SI4732: {
      auto *si4732 = static_cast<SI4732Driver *>(driver);
      uint16_t rssi = si4732->readRSSI();
      signalDetected = (rssi > SCAN_RSSI_THRESHOLD);
      break;
    }
    case RadioType::BK1080: {
      auto *bk1080 = static_cast<BK1080Driver *>(driver);
      uint16_t rssi = bk1080->readRSSI();
      signalDetected = (rssi > SCAN_RSSI_THRESHOLD);
      break;
    }
    default:
      break;
    }

    if (signalDetected) {
      // Нашли сигнал - останавливаем сканирование
      stopScan();
      switchVFO(scanIndex_);
    } else {
      scanNext();
    }
  }

  // ========================================================================
  // VFO MANAGEMENT
  // ========================================================================

  void switchVFO(uint8_t index) {
    if (index >= MAX_VFO_COUNT || !vfos_[index])
      return;

    activeVFO_ = index;
    applyVFOToHardware(activeVFO_);
  }

  IRadioDriver *active() const { return vfos_[activeVFO_]; }

  uint8_t getActiveIndex() const { return activeVFO_; }

  // ========================================================================
  // SCANNING
  // ========================================================================

  void startScan() {
    scanning_ = true;
    scanIndex_ = 0;
    scanNext();
  }

  void stopScan() { scanning_ = false; }

  bool isScanning() const { return scanning_; }

  // ========================================================================
  // ACCESS
  // ========================================================================

  IRadioDriver *operator[](uint8_t index) {
    return (index < vfoCount_) ? vfos_[index] : nullptr;
  }

  IRadioDriver *active() {
    return (activeVFO_ < vfoCount_) ? vfos_[activeVFO_] : nullptr;
  }

  uint8_t getCount() const { return vfoCount_; }

  void setActive(uint8_t index) {
    if (index >= vfoCount_)
      return;

    Log("VFO SET ACTIVE %u %u", activeVFO_, vfos_[activeVFO_]);

    if (activeVFO_ < vfoCount_) {
      vfos_[activeVFO_]->setStandby();
    }

    activeVFO_ = index;
    vfos_[activeVFO_]->setRxMode();
  }

private:
  static constexpr uint8_t MAX_VFO_COUNT = 8;
  static constexpr uint16_t SCAN_RSSI_THRESHOLD = 100;

  IRadioDriver *vfos_[MAX_VFO_COUNT];
  uint8_t activeVFO_;
  bool scanning_;
  uint8_t scanIndex_;
  uint8_t vfoCount_;

  BK1080Driver bk1080_;
  BK4819Driver bk4819_;
  SI4732Driver si4732_;

  void scanNext() {
    do {
      scanIndex_ = (scanIndex_ + 1) % MAX_VFO_COUNT;
    } while (!vfos_[scanIndex_] && scanIndex_ != activeVFO_);

    if (vfos_[scanIndex_]) {
      applyVFOToHardware(scanIndex_);
    }
  }
};
