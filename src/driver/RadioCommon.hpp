#pragma once

#include "../external/printf/printf.h"
#include <cstdint>
#include <type_traits>

constexpr uint32_t operator""_kHz(long double f) {
  return static_cast<uint32_t>(f * 100.0);
}

constexpr uint32_t operator""_MHz(long double f) {
  return static_cast<uint32_t>(f * 100000.0);
}

// ============================================================================
// RADIO TYPES
// ============================================================================

enum class RadioType : uint8_t { BK4819, SI4732, BK1080, COUNT };

enum class ModType : uint8_t {
  FM = 0,
  AM = 1,
  USB = 2,
  LSB = 3,
  BYP = 4,
  RAW = 5,
  WFM = 6
};

enum class TxState : uint8_t {
  TX_OFF = 0,      // Передача выключена
  TX_ON = 1,       // Передача активна
  TX_CHARGING = 2, // Идёт зарядка
  TX_BAT_LOW = 3,  // Низкий заряд батареи
  TX_DISABLED = 4, // Передача запрещена
  TX_UPCONV = 5,   // Активен upconverter
  TX_HIGH_POW = 6  // Высокое напряжение
};

// ============================================================================
// PARAMETER IDS
// ============================================================================

enum class ParamId : uint8_t {
  Frequency,
  TxFrequency,
  TxState,
  TxPower,
  Channel,
  Gain,
  Bandwidth,
  Modulation,
  Squelch,
  Step,
  Volume,
  PowerState,
  RxMode,
  Mute,
  RSSI,
  Noise,
  Glitch,
  SNR,
  SquelchOpen,
  PowerOn,
  PowerOff,
  StartTX,
  StopTX,
  Radio,
  Count
};

// ============================================================================
// PARAMETER FLAGS
// ============================================================================

constexpr uint8_t PARAM_PERSIST = 0x01; // Сохранять в EEPROM
constexpr uint8_t PARAM_READONLY = 0x02; // Только чтение (RSSI, SNR, etc)
constexpr uint8_t PARAM_ACTION = 0x04; // Действие, не значение
constexpr uint8_t PARAM_REQUIRES_POWER = 0x08; // Требует включенного питания

// ============================================================================
// UNIFIED PARAMETER CLASS (без отдельного ParamProxy)
// ============================================================================

template <typename T = uint32_t> class Param {
public:
  using ApplyFunc = void (*)(void *context, T value);
  using GetMinFunc = T (*)(void *context);
  using GetMaxFunc = T (*)(void *context);

  Param()
      : value_(0), min_(0), max_(0), flags_(0), dirty_(false),
        context_(nullptr), apply_(nullptr), getMin_(nullptr), getMax_(nullptr) {
  }

  Param(T value, T min, T max, uint8_t flags = 0)
      : value_(value), min_(min), max_(max), flags_(flags), dirty_(false),
        context_(nullptr), apply_(nullptr), getMin_(nullptr), getMax_(nullptr) {
  }

  // Конструктор с apply функцией
  Param(T value, T min, T max, void *context, ApplyFunc apply,
        uint8_t flags = 0)
      : value_(value), min_(min), max_(max), flags_(flags), dirty_(false),
        context_(context), apply_(apply), getMin_(nullptr), getMax_(nullptr) {}

  // Конструктор с динамическими границами
  Param(T value, void *context, GetMinFunc getMin, GetMaxFunc getMax,
        ApplyFunc apply = nullptr, uint8_t flags = 0)
      : value_(value), min_(0), max_(0), flags_(flags), dirty_(false),
        context_(context), apply_(apply), getMin_(getMin), getMax_(getMax) {}

  // Получение значения
  T get() const { return value_; }

  // Установка значения с валидацией и применением
  bool set(T newValue, bool force = false) {
    if (flags_ & PARAM_READONLY && !force) {
      return false;
    }

    T minVal = getMin_ ? getMin_(context_) : min_;
    T maxVal = getMax_ ? getMax_(context_) : max_;

    // Безопасная валидация границ
    if (newValue < minVal)
      newValue = minVal;
    if (newValue > maxVal)
      newValue = maxVal;

    if (value_ == newValue && !force) {
      return false;
    }

    value_ = newValue;
    dirty_ = true;

    if (apply_) {
      apply_(context_, value_);
    }

    return true;
  }

  // Операторы для удобной работы
  Param &operator=(T newValue) {
    set(newValue);
    return *this;
  }

  // Безопасное сложение с проверкой переполнения
  Param &operator+=(T val) {
    T maxVal = getMax_ ? getMax_(context_) : max_;
    if (value_ <= maxVal - val) {
      set(value_ + val);
    } else {
      set(maxVal);
    }
    return *this;
  }

  // Безопасное вычитание с проверкой переполнения
  Param &operator-=(T val) {
    T minVal = getMin_ ? getMin_(context_) : min_;
    if (value_ >= minVal + val) {
      set(value_ - val);
    } else {
      set(minVal);
    }
    return *this;
  }

  operator T() const { return value_; }

  bool isDirty() const { return dirty_; }
  void clearDirty() { dirty_ = false; }
  uint8_t flags() const { return flags_; }

  T getMin() const { return getMin_ ? getMin_(context_) : min_; }
  T getMax() const { return getMax_ ? getMax_(context_) : max_; }

private:
  T value_;
  T min_;
  T max_;
  uint8_t flags_;
  bool dirty_;
  void *context_;
  ApplyFunc apply_;
  GetMinFunc getMin_;
  GetMaxFunc getMax_;
};

// Специализация для перечислений
template <typename E> using EnumParam = Param<std::underlying_type_t<E>>;
