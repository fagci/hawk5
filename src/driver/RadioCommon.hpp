#pragma once
#include <cstdint>

constexpr uint32_t operator"" _kHz(long double f) {
  return static_cast<uint32_t>(f * 100.0);
}

constexpr uint32_t operator"" _MHz(long double f) {
  return static_cast<uint32_t>(f * 100000.0);
}

// ============================================================================
// COMMON RADIO TYPES
// ============================================================================

enum class RadioPowerState : uint8_t { OFF, STANDBY, RX, TX };

enum class RadioType : uint8_t { BK4819, SI4732, BK1080, COUNT };

// Modulation types
enum ModulationType : uint8_t {
  MOD_FM = 0,
  MOD_AM = 1,
  MOD_USB = 2,
  MOD_LSB = 3,
  MOD_BYP = 4,
  MOD_RAW = 5,
  MOD_WFM = 6
};

// Filter types
enum Filter : uint8_t {
  FILTER_VHF = 0,
  FILTER_UHF = 1,
  FILTER_OFF = 2,
  FILTER_AUTO = 3
};

struct VFOState {
  RadioType radioType = RadioType::BK4819;
  uint32_t frequency = 145500000;
  uint32_t gain = 32;
  uint32_t bandwidth = 1;
  uint32_t modulation = 0;
  uint32_t squelch = 4;
  char name[10] = "VFO-A";
  bool dirty = false;
  bool enabled = true; // Для сканирования
  /*
    void save(uint16_t index) {
      // EEPROM_Write(EEPROM_VFO_BASE + index * sizeof(VFOState), this,
      // sizeof(VFOState));
      dirty = false;
    }

    void load(uint16_t index) {
      // EEPROM_Read(EEPROM_VFO_BASE + index * sizeof(VFOState), this,
      // sizeof(VFOState));
      dirty = false;
    } */
};

template <typename ApplyFunc> class CachedParamProxyT {
public:
  CachedParamProxyT(uint32_t &cache, ApplyFunc apply, uint32_t min,
                    uint32_t max, bool &dirty)
      : cache_(cache), apply_(apply), min_(min), max_(max), dirty_(dirty) {}

  // Operator=
  CachedParamProxyT &operator=(uint32_t value) {
    if (value >= min_ && value <= max_ && cache_ != value) {
      cache_ = value;
      dirty_ = true;
      apply_(value); // Применить к железу
    }
    return *this;
  }

  // Implicit conversion
  operator uint32_t() const { return cache_; }

  // Операторы инкремента/декремента
  CachedParamProxyT &operator++() {
    if (cache_ < max_) {
      cache_++;
      dirty_ = true;
      apply_(cache_);
    }
    return *this;
  }

  CachedParamProxyT &operator--() {
    if (cache_ > min_) {
      cache_--;
      dirty_ = true;
      apply_(cache_);
    }
    return *this;
  }

  CachedParamProxyT &operator+=(uint32_t val) {
    if (cache_ + val <= max_) {
      cache_ += val;
      dirty_ = true;
      apply_(cache_);
    }
    return *this;
  }

  CachedParamProxyT &operator-=(uint32_t val) {
    if (cache_ >= min_ + val) {
      cache_ -= val;
      dirty_ = true;
      apply_(cache_);
    }
    return *this;
  }

private:
  uint32_t &cache_;
  ApplyFunc apply_;
  uint32_t min_;
  uint32_t max_;
  bool &dirty_;
};
