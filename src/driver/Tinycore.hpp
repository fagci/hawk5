#ifndef BK4819_MICRO_CORE_H
#define BK4819_MICRO_CORE_H

#include "../driver/gpio.h"
#include "../inc/dp32g030/gpio.h"
#include <cstdint>
#include <functional>

// ============================================================================
// User-defined literals
// ============================================================================

constexpr uint32_t operator""_kHz(unsigned long long khz) { return khz * 100; }
constexpr uint32_t operator""_MHz(unsigned long long mhz) {
  return mhz * 100000;
}

// ============================================================================
// Enums
// ============================================================================

enum Mod : uint8_t { FM, AM, USB, LSB, BYP, RAW, WFM };
enum Flt : uint8_t { VHF, UHF, FLT_OFF, FLT_AUTO };

// ============================================================================
// BK4819 - Fluent API с chainable методами
// ============================================================================

struct BK4819 {
  // Hardware interface
  std::function<uint16_t(uint8_t)> rd;
  std::function<void(uint8_t, uint16_t)> wr;
  std::function<void(uint32_t)> dly;

  // State
  struct {
    uint32_t freq = 0;
    uint16_t gpio = 0x9000;
    uint8_t flt = 0xFF;
    bool rx = false, tx = false;
  } state;

  // Constants
  static constexpr uint8_t MOD_VAL[] = {2, 7, 5, 5, 9, 4, 2};
  static constexpr uint16_t GAIN_TBL[] = {
      0x10,  0x1,   0x9,   0x2,   0xA,   0x12,  0x2A,  0x32,
      0x3A,  0x20B, 0x213, 0x21B, 0x214, 0x21C, 0x22D, 0x23C,
      0x23D, 0x255, 0x25D, 0x275, 0x295, 0x2B6, 0x354, 0x36C,
      0x38C, 0x38D, 0x3B5, 0x3B6, 0x3D6, 0x3BF, 0x3DF, 0x3FF};

  struct BwCfg {
    uint8_t rf, af, bs;
  };
  static constexpr BwCfg BW_TBL[] = {{0, 1, 1}, {1, 2, 1}, {1, 0, 0}, {3, 0, 0},
                                     {1, 0, 2}, {2, 0, 2}, {3, 3, 2}, {4, 6, 2},
                                     {5, 7, 2}, {7, 4, 2}};

  // ========================================================================
  // Constructor
  // ========================================================================

  BK4819(uint16_t (*r)(uint8_t), void (*w)(uint8_t, uint16_t),
         void (*d)(uint32_t) = nullptr)
      : rd([r](uint8_t x) { return r(x); }),
        wr([w](uint8_t x, uint16_t v) { w(x, v); }), dly(d ? d : nullptr) {}

  // ========================================================================
  // Chainable configuration
  // ========================================================================

  BK4819 &init() {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
    wr(0x00, 0x8000);
    wr(0x00, 0x0000);
    wr(0x37, 0x1D0F);
    wr(0x36, 0x0022);
    agc(20);
    wr(0x48, (11u << 12) | (0 << 10) | (58 << 4) | (8 << 0));
    wr(0x7D, 0xE94F);
    wr(0x19, 0x1041);
    state.gpio = 0x9000;
    wr(0x33, state.gpio);
    if (dly)
      while (rd(0x0C) & 1) {
        wr(0x02, 0);
        dly(1);
      }
    return *this;
  }

  BK4819 &rx(bool on = true) {
    state.rx = on;
    if (on) {
      wr(0x36, 0x0000);
      wr(0x37, 0x1F0F);
      wr(0x30, 0x0200);
      wr(0x30, (1u << 15) | (0u << 14) | (1u << 10) | (1u << 9) | (1u << 8) |
                   (1u << 6) | (0u << 5) | (0u << 4) | (0u << 3) | (1u << 0));
    } else {
      wr(0x30, 0x0000);
    }
    return *this;
  }

  BK4819 &tx(bool on = true) {
    state.tx = on;
    if (on) {
      wr(0x37, 0x1D0F);
      wr(0x30, 0xC1FE);
    } else {
      wr(0x30, 0x0000);
    }
    return *this;
  }

  BK4819 &freq(uint32_t f) {
    state.freq = f;
    wr(0x38, f & 0xFFFF);
    wr(0x39, (f >> 16) & 0xFFFF);

    // Auto filter
    state.flt = (f < 28000000) ? VHF : UHF;
    const uint16_t VHF_BIT = 0x40U >> 4, UHF_BIT = 0x40U >> 3;
    if (state.flt == VHF) {
      state.gpio |= VHF_BIT;
      state.gpio &= ~UHF_BIT;
    } else {
      state.gpio &= ~VHF_BIT;
      state.gpio |= UHF_BIT;
    }
    wr(0x33, state.gpio);

    // VCO cal
    uint16_t r30 = rd(0x30);
    wr(0x30, 0x0200);
    wr(0x30, r30);
    return *this;
  }

  BK4819 &mod(uint8_t m) {
    if (m > 6)
      m = 0;
    wr(0x47, 0x6040 | (MOD_VAL[m] << 8));

    const uint16_t xtal[] = {20232, 20296, 20360, 20424};
    const uint16_t ifset[] = {0x3555, 0x2E39, 0x2AAB, 0x271C};
    uint8_t x = (m == WFM) ? 0 : (m == USB || m == LSB) ? 3 : 2;
    wr(0x3C, xtal[x]);
    wr(0x3D, ifset[x]);

    if (m == WFM)
      bw(9);
    return *this;
  }

  BK4819 &bw(uint8_t b) {
    if (b > 9)
      b = 9;
    b = 9 - b;
    const BwCfg &c = BW_TBL[b];
    wr(0x43,
       (c.rf << 12) | (c.rf << 9) | (c.af << 6) | (c.bs << 4) | (1u << 3));
    return *this;
  }

  BK4819 &agc(uint8_t idx) {
    bool auto_ = (idx == 20);
    uint16_t r7e = rd(0x7E);
    r7e &= ~((1 << 15) | (0b111 << 12));
    r7e |= (!auto_ << 15) | (3u << 12) | (5u << 3) | (6u << 0);
    wr(0x7E, r7e);
    wr(0x13, auto_ ? 0x03BE : (idx < 32 ? GAIN_TBL[idx] : 0x3FF));
    wr(0x12, (3u << 8) | (3u << 5) | (3u << 3) | (4u << 0));
    wr(0x11, (2u << 8) | (3u << 5) | (3u << 3) | (3u << 0));
    wr(0x10, (0u << 8) | (3u << 5) | (3u << 3) | (2u << 0));
    wr(0x14, (0u << 8) | (0u << 5) | (3u << 3) | (1u << 0));
    wr(0x49, (0 << 14) | (50 << 7) | (20 << 0));
    wr(0x7B, 0x318C);
    return *this;
  }

  BK4819 &sql(uint16_t lvl) {
    wr(0x4E, (1u << 14) | (3 << 11) | (3 << 9) | (lvl & 0x1FF));
    return *this;
  }

  BK4819 &filter(uint8_t f) {
    if (state.flt == f)
      return *this;
    state.flt = f;
    const uint16_t VHF_BIT = 0x40U >> 4, UHF_BIT = 0x40U >> 3;
    if (f == VHF) {
      state.gpio |= VHF_BIT;
      state.gpio &= ~UHF_BIT;
    } else if (f == UHF) {
      state.gpio &= ~VHF_BIT;
      state.gpio |= UHF_BIT;
    } else {
      state.gpio &= ~(VHF_BIT | UHF_BIT);
    }
    wr(0x33, state.gpio);
    return *this;
  }

  // ========================================================================
  // Getters (read-only)
  // ========================================================================

  uint16_t rssi() const { return rd(0x67) & 0x1FF; }
  uint8_t noise() const { return rd(0x65) & 0x7F; }
  uint8_t snr() const { return rd(0x61) & 0xFF; }
  uint8_t glitch() const { return rd(0x63) & 0xFF; }
  uint16_t gain() const { return rd(0x13) & 0x3FF; }
  bool sqOpen() const { return (rd(0x0C) >> 1) & 1; }
  uint32_t getFreq() const { return state.freq; }

  // ========================================================================
  // Scanning helper
  // ========================================================================

  template <typename Fn>
  BK4819 &scan(uint32_t start, uint32_t end, uint32_t step, Fn callback) {
    for (uint32_t f = start; f <= end; f += step) {
      freq(f);
      if (dly)
        dly(1);
      if (!callback(f, *this))
        break;
    }
    return *this;
  }
};

// ============================================================================
// Helper function
// ============================================================================

inline BK4819 makeBK(uint16_t (*r)(uint8_t), void (*w)(uint8_t, uint16_t),
                     void (*d)(uint32_t) = nullptr) {
  return BK4819(r, w, d);
}

// ============================================================================
// ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ
// ============================================================================

/*
#include <cstdio>

extern uint16_t BK4819_ReadRegister(uint8_t);
extern void BK4819_WriteRegister(uint8_t, uint16_t);
extern void TIMER_DelayMs(uint32_t);

// ============================================================================
// Пример 1: Fluent API (chainable)
// ============================================================================

void example1() {
    auto bk = makeBK(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs);

    // Настройка в одну цепочку
    bk.init()
      .rx()
      .freq(433_MHz)
      .mod(FM)
      .bw(3)
      .agc(20)
      .sql(100);

    // Чтение
    printf("RSSI: %u, SNR: %u\n", bk.rssi(), bk.snr());
}

// ============================================================================
// Пример 2: Сканирование с callback
// ============================================================================

void example2() {
    auto bk = makeBK(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs);

    bk.init().rx().mod(FM).bw(3);

    // Сканирование с лямбдой
    bk.scan(170_MHz, 176_MHz, 25_kHz, [](uint32_t f, const BK4819& radio) {
        printf("Freq: %u Hz, RSSI: %u\n", f, radio.rssi());
        return true; // continue scanning
    });
}

// ============================================================================
// Пример 3: Ручной цикл
// ============================================================================

void example3() {
    auto bk = makeBK(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs);

    bk.init().rx().mod(FM).bw(3);

    for (uint32_t f = 170_MHz; f <= 176_MHz; f += 25_kHz) {
        bk.freq(f);
        TIMER_DelayMs(1);

        if (bk.rssi() > 100) {
            printf("Strong signal at %u MHz\n", f / 1000000);
        }
    }
}

// ============================================================================
// Пример 4: Изменение параметров на лету
// ============================================================================

void example4() {
    auto bk = makeBK(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs);

    bk.init()
      .rx()
      .freq(145_MHz)
      .mod(FM);

    // Быстрое переключение
    bk.freq(433_MHz);           // Новая частота
    bk.mod(AM);                 // Новая модуляция
    bk.bw(5);                   // Новая полоса

    // Или цепочкой
    bk.freq(146_MHz).mod(FM).bw(3);
}

// ============================================================================
// Пример 5: TX/RX переключение
// ============================================================================

void example5() {
    auto bk = makeBK(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs);

    bk.init()
      .freq(433_MHz)
      .mod(FM);

    // RX режим
    bk.rx();
    TIMER_DelayMs(1000);

    // TX режим
    bk.rx(false).tx();
    TIMER_DelayMs(100);

    // Обратно в RX
    bk.tx(false).rx();
}

// ============================================================================
// Пример 6: Поиск активных каналов
// ============================================================================

void example6() {
    auto bk = makeBK(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs);

    bk.init().rx().mod(FM).bw(3).sql(50);

    bk.scan(400_MHz, 470_MHz, 25_kHz, [](uint32_t f, const BK4819& radio) {
        if (radio.sqOpen()) {
            printf("ACTIVITY at %u.%03u MHz (RSSI: %u)\n",
                   f / 1000000, (f / 1000) % 1000, radio.rssi());
        }
        return true;
    });
}

// ============================================================================
// Пример 7: Класс-обертка
// ============================================================================

class MyRadio {
    BK4819 bk;

public:
    MyRadio() : bk(BK4819_ReadRegister, BK4819_WriteRegister, TIMER_DelayMs) {
        bk.init().rx().mod(FM).bw(3);
    }

    void tune(uint32_t f) { bk.freq(f); }
    uint16_t getRSSI() { return bk.rssi(); }
    bool isSignal() { return bk.sqOpen(); }

    void scanBand() {
        bk.scan(144_MHz, 146_MHz, 25_kHz, [](uint32_t f, const BK4819& r) {
            printf("%u.%03u MHz: %u\n",
                   f/1000000, (f/1000)%1000, r.rssi());
            return true;
        });
    }
};
*/

#endif // BK4819_MICRO_CORE_H
