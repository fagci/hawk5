#pragma once

#include "../helper/channels.h"
#include "RadioCommon.hpp"
#include "bk4819.h"

// ============================================================================
// RADIO DRIVER INTERFACE
// ============================================================================

class IRadioDriver {
public:
  IRadioDriver()
      : channelIndex_(0xFFFF), powerState_(0), inTransaction_(false) {
    // Инициализация пустыми параметрами
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      params_[i] = Param<uint32_t>();
    }
  }

  virtual ~IRadioDriver() = default;

  virtual RadioType getRadioType() const = 0;
  virtual const char *getRadioName() const = 0;

  // Helper для доступа к параметрам (избавляемся от каста)
  inline Param<uint32_t> &getParam(ParamId id) { return params_[(uint8_t)id]; }

  inline const Param<uint32_t> &getParam(ParamId id) const {
    return params_[(uint8_t)id];
  }

  // Оператор для удобного доступа
  Param<uint32_t> &operator[](ParamId id) { return getParam(id); }

  // Проверка наличия грязных параметров для сохранения
  virtual bool isDirty() const {
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      if (params_[i].isDirty() && (params_[i].flags() & PARAM_PERSIST)) {
        return true;
      }
    }
    return false;
  }

  virtual void clearDirty() {
    for (uint8_t i = 0; i < (uint8_t)ParamId::Count; ++i) {
      params_[i].clearDirty();
    }
  }

  // === BATCH OPERATIONS (транзакции) ===

  void beginTransaction() { inTransaction_ = true; }

  void endTransaction() {
    inTransaction_ = false;
    // Здесь драйвер может применить батч изменений
    applyPendingChanges();
  }

  bool inTransaction() const { return inTransaction_; }

  // === CHANNEL OPERATIONS ===
  virtual void loadCh(uint16_t chNum, MR *ch) {
    channelIndex_ = chNum;
    if (chNum == 0xFFFF)
      return;

    uint32_t freq = ch->rxF;
    uint32_t mod = ch->modulation;
    uint32_t bw = ch->bw;
    uint32_t gain = ch->gainIndex;
    uint32_t squelch = ch->squelch.value;
    uint32_t step = ch->step;

    // Автоматически загружаем все persistable параметры
    loadParamIfPersist(ParamId::Frequency, freq);
    loadParamIfPersist(ParamId::Modulation, mod);
    loadParamIfPersist(ParamId::Bandwidth, bw);
    loadParamIfPersist(ParamId::Gain, gain);
    loadParamIfPersist(ParamId::Squelch, squelch);
    loadParamIfPersist(ParamId::Step, step);

    clearDirty();
  }

  // Автоматизированная загрузка параметров с флагом PARAM_PERSIST
  virtual void loadCh(uint16_t chNum) {
    channelIndex_ = chNum;
    if (chNum == 0xFFFF)
      return;

    MR ch;
    CHANNELS_Load(chNum, &ch);

    loadCh(chNum, &ch);
  }

  // Автоматизированное сохранение параметров с флагом PARAM_PERSIST
  virtual void saveCh() {
    if (channelIndex_ == 0xFFFF)
      return;

    MR ch;
    CHANNELS_Load(channelIndex_, &ch);

    uint32_t freq = ch.rxF;
    uint32_t mod = ch.modulation;
    uint32_t bw = ch.bw;
    uint32_t gain = ch.gainIndex;
    uint32_t squelch = ch.squelch.value;
    uint32_t step = ch.step;

    // Автоматически сохраняем все persistable параметры
    saveParamIfDirty(ParamId::Frequency, freq);
    saveParamIfDirty(ParamId::Modulation, mod);
    saveParamIfDirty(ParamId::Bandwidth, bw);
    saveParamIfDirty(ParamId::Gain, gain);
    saveParamIfDirty(ParamId::Squelch, squelch);
    saveParamIfDirty(ParamId::Step, step);

    // Записываем обратно в структуру
    ch.rxF = freq;
    ch.modulation = (ModulationType)mod;
    ch.bw = (BK4819_FilterBandwidth_t)bw;
    ch.gainIndex = gain;
    ch.squelch.value = squelch;
    ch.step = (Step)step;

    CHANNELS_Save(channelIndex_, &ch);
    clearDirty();
  }

  uint16_t getChannelIndex() const { return channelIndex_; }

protected:
  Param<uint32_t> params_[(uint8_t)ParamId::Count];
  uint16_t channelIndex_;
  uint8_t powerState_;
  bool inTransaction_;

  virtual void applyPendingChanges() {
    // Переопределяется в конкретных драйверах при необходимости
  }

  // Helper для загрузки параметра, если он persistable
  inline void loadParamIfPersist(ParamId id, uint32_t &value) {
    auto &param = getParam(id);
    if (param.flags() & PARAM_PERSIST) {
      param.set(value, true); // force=true для загрузки
    }
  }

  // Helper для сохранения параметра, если он dirty и persistable
  inline void saveParamIfDirty(ParamId id, uint32_t &value) {
    auto &param = getParam(id);
    if (param.isDirty() && (param.flags() & PARAM_PERSIST)) {
      value = param.get();
    }
  }
};
