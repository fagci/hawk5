#pragma once
#include <cstdint>
#include <functional>

constexpr uint32_t operator"" _kHz(long double f) {
  return static_cast<uint32_t>(f * 100.0);
}

constexpr uint32_t operator"" _MHz(long double f) {
  return static_cast<uint32_t>(f * 100000.0);
}

// ============================================================================
// RADIO TYPES
// ============================================================================
enum class RadioType : uint8_t { BK4819, SI4732, BK1080, COUNT };

enum ModulationType : uint8_t {
  MOD_FM = 0,
  MOD_AM = 1,
  MOD_USB = 2,
  MOD_LSB = 3,
  MOD_BYP = 4,
  MOD_RAW = 5,
  MOD_WFM = 6
};

// ============================================================================
// PARAMETER IDS
// ============================================================================
enum class ParamId : uint8_t {
  Frequency,
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
  Count
};

// ============================================================================
// PARAMETER FLAGS
// ============================================================================
enum ParamFlags : uint8_t {
  PARAM_READABLE = 1 << 0,
  PARAM_WRITABLE = 1 << 1,
  PARAM_ACTION = 1 << 2,
  PARAM_PERSIST = 1 << 3,
};

// ============================================================================
// COMMON RADIO TYPES
// ============================================================================

enum class RadioPowerState : uint8_t { OFF, STANDBY, RX, TX };

// Filter types
enum Filter : uint8_t {
  FILTER_VHF = 0,
  FILTER_UHF = 1,
  FILTER_OFF = 2,
  FILTER_AUTO = 3
};

// ============================================================================
// PARAM PROXY с динамическими границами
// ============================================================================
template <typename ApplyFunc> class ParamProxy {
public:
  ParamProxy(uint32_t &value, ApplyFunc apply, std::function<uint32_t()> getMin,
             std::function<uint32_t()> getMax, bool &dirty, const char *name,
             uint8_t flags)
      : value_(value), apply_(apply), getMin_(getMin), getMax_(getMax),
        dirty_(dirty), name_(name), flags_(flags) {}

  ParamProxy &operator=(uint32_t val) {
    uint32_t minVal = getMin_();
    uint32_t maxVal = getMax_();
    if (val >= minVal && val <= maxVal) {
      value_ = val;
      dirty_ = true;
      apply_(val);
    }
    return *this;
  }

  operator uint32_t() const { return value_; }

  ParamProxy &operator+=(uint32_t val) {
    if (value_ + val <= getMax_()) {
      value_ += val;
      dirty_ = true;
      apply_(value_);
    }
    return *this;
  }

  ParamProxy &operator-=(uint32_t val) {
    if (value_ >= getMin_() + val) {
      value_ -= val;
      dirty_ = true;
      apply_(value_);
    }
    return *this;
  }

  ParamProxy &operator++() {
    if (value_ < getMax_()) {
      ++value_;
      dirty_ = true;
      apply_(value_);
    }
    return *this;
  }

  ParamProxy &operator--() {
    if (value_ > getMin_()) {
      --value_;
      dirty_ = true;
      apply_(value_);
    }
    return *this;
  }

  // Метаданные - теперь динамические!
  const char *name() const { return name_; }
  uint32_t min() const { return getMin_(); }
  uint32_t max() const { return getMax_(); }
  uint8_t flags() const { return flags_; }
  uint32_t get() const { return value_; }

  bool isReadable() const { return flags_ & PARAM_READABLE; }
  bool isWritable() const { return flags_ & PARAM_WRITABLE; }
  bool isAction() const { return flags_ & PARAM_ACTION; }
  bool shouldPersist() const { return flags_ & PARAM_PERSIST; }

private:
  uint32_t &value_;
  ApplyFunc apply_;
  std::function<uint32_t()> getMin_;
  std::function<uint32_t()> getMax_;
  bool &dirty_;
  const char *name_;
  uint8_t flags_;
};

// ============================================================================
// PARAMETER с динамическими границами
// ============================================================================
template <typename ApplyFunc = std::function<void(uint32_t)>> class Param {
public:
  Param()
      : name_(nullptr), value_(0), apply_([](uint32_t) {}),
        getMin_([]() { return 0u; }), getMax_([]() { return 0u; }), flags_(0),
        dirty_(false) {}

  // Конструктор с лямбдами для min/max
  Param(const char *name, uint32_t initial, ApplyFunc apply,
        std::function<uint32_t()> getMin, std::function<uint32_t()> getMax,
        uint8_t flags = PARAM_READABLE | PARAM_WRITABLE)
      : name_(name), value_(initial), apply_(apply), getMin_(getMin),
        getMax_(getMax), flags_(flags), dirty_(false) {}

  // Конструктор со статическими границами (для совместимости)
  Param(const char *name, uint32_t initial, ApplyFunc apply, uint32_t min,
        uint32_t max, uint8_t flags = PARAM_READABLE | PARAM_WRITABLE)
      : name_(name), value_(initial), apply_(apply),
        getMin_([min]() { return min; }), getMax_([max]() { return max; }),
        flags_(flags), dirty_(false) {}

  // Без имени
  Param(uint32_t initial, ApplyFunc apply, uint32_t min, uint32_t max,
        uint8_t flags = PARAM_READABLE | PARAM_WRITABLE)
      : name_(nullptr), value_(initial), apply_(apply),
        getMin_([min]() { return min; }), getMax_([max]() { return max; }),
        flags_(flags), dirty_(false) {}

  ParamProxy<ApplyFunc> proxy() {
    return ParamProxy<ApplyFunc>(value_, apply_, getMin_, getMax_, dirty_,
                                 name_, flags_);
  }

  uint32_t get() const { return value_; }
  const char *name() const { return name_; }
  uint32_t min() const { return getMin_(); }
  uint32_t max() const { return getMax_(); }
  uint8_t flags() const { return flags_; }
  bool isDirty() const { return dirty_; }

  void set(uint32_t val) {
    uint32_t minVal = getMin_();
    uint32_t maxVal = getMax_();
    if (val >= minVal && val <= maxVal) {
      value_ = val;
      if (flags_ & PARAM_WRITABLE) {
        dirty_ = true;
        apply_(val);
      }
    }
  }

  // Тихий set - без применения (для loadCh)
  void setQuiet(uint32_t val) {
    uint32_t minVal = getMin_();
    uint32_t maxVal = getMax_();
    if (val >= minVal && val <= maxVal) {
      value_ = val;
      // НЕ вызываем apply_!
    }
  }

  void clearDirty() { dirty_ = false; }

private:
  const char *name_;
  uint32_t value_;
  ApplyFunc apply_;
  std::function<uint32_t()> getMin_;
  std::function<uint32_t()> getMax_;
  uint8_t flags_;
  bool dirty_;
};
