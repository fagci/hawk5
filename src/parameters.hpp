#pragma once
#include "./external/printf/printf.h"
#include "driver/bk4819.h"
#include <cstdint>
#include <stddef.h>

constexpr uint32_t operator"" _kHz(long double f) {
  return static_cast<uint32_t>(f * 100.0);
}
constexpr uint32_t operator"" _MHz(long double f) {
  return static_cast<uint32_t>(f * 100000.0);
}

// ============================================================================
// УНИВЕРСАЛЬНЫЙ ПАРАМЕТР (работает с ЛЮБЫМИ полями!)
// ============================================================================

struct Param {
  const char *name;

  // Getter/Setter (для битовых полей)
  uint32_t (*get)();
  void (*set)(uint32_t value);

  uint32_t min;
  uint32_t max;
  const char *const *stringTable; // nullptr если нет

  // Inline методы
  uint32_t getValue() const { return get(); }

  void setValue(uint32_t v) {
    if (v >= min && v <= max) {
      set(v);
    }
  }

  void inc() {
    uint32_t v = get();
    if (v < max)
      set(v + 1);
  }

  void dec() {
    uint32_t v = get();
    if (v > min)
      set(v - 1);
  }

  const char *toString() const {
    if (stringTable) {
      uint32_t v = get();
      if (v <= max)
        return stringTable[v];
    }
    static char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)get());
    return buf;
  }
};

// ============================================================================
// МАГИЧЕСКИЕ МАКРОСЫ ДЛЯ АВТОГЕНЕРАЦИИ
// ============================================================================

// Для обычных полей (uint8_t, uint32_t и т.д.)
#define PARAM_FIELD(name_str, struct_var, field, min_val, max_val)             \
  {                                                                            \
    name_str, []() -> uint32_t { return struct_var.field; },                   \
        [](uint32_t v) { struct_var.field = v; }, min_val, max_val, nullptr    \
  }

// Для битовых полей (те же макросы работают!)
#define PARAM_BITFIELD(name_str, struct_var, field, min_val, max_val)          \
  {                                                                            \
    name_str, []() -> uint32_t { return struct_var.field; },                   \
        [](uint32_t v) { struct_var.field = v; }, min_val, max_val, nullptr    \
  }

// С enum таблицей
#define PARAM_ENUM(name_str, struct_var, field, strings, count)                \
  {                                                                            \
    name_str, []() -> uint32_t { return struct_var.field; },                   \
        [](uint32_t v) { struct_var.field = v; }, 0, count - 1, strings        \
  }

static const Param radioParams[] = {
    (Param){
        "Frequency",
        []() -> uint32_t { return BK4819_GetFrequency(); },
        [](uint32_t f) { BK4819_TuneTo(f, true); },
        BK4819_F_MIN,
        BK4819_F_MAX,
    },
    (Param){"RSSI", []() -> uint32_t { return BK4819_GetRSSI(); }},
    (Param){"Noise", []() -> uint32_t { return BK4819_GetNoise(); }},
    (Param){"Glitch", []() -> uint32_t { return BK4819_GetGlitch(); }},
    (Param){"SNR", []() -> uint32_t { return BK4819_GetSNR(); }},
};
