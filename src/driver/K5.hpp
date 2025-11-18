#pragma once
#include "../inc/dp32g030/gpio.h"
#include "bk4819-regs.h" // Для BK4819_REG_33
#include "bk4819.h"
#include "systick.h"
#include "uart.h"
#include <cstdint>

// ============================================================================
// HAL Layer - Универсальные примитивы
// ============================================================================

template <uint32_t PortBase, uint8_t PinNum> struct Pin {
  static constexpr volatile GPIO_Bank_t *port() {
    return reinterpret_cast<volatile GPIO_Bank_t *>(PortBase);
  }

  static void set() { port()->DATA |= (1U << PinNum); }
  static void clear() { port()->DATA &= ~(1U << PinNum); }
  static void toggle() { port()->DATA ^= (1U << PinNum); }
  static bool read() { return (port()->DATA >> PinNum) & 1U; }

  static void setOutput() { port()->DIR |= (1U << PinNum); }
  static void setInput() { port()->DIR &= ~(1U << PinNum); }
};

// ============================================================================
// BK4819 GPIO Pin - управление через SPI регистры
// ============================================================================

inline uint16_t gGpioOutState = 0x9000;

enum class BK4819_GPIO_PIN : uint8_t {
  GPIO0, // GREEN
  GPIO1, // RED
  GPIO2, // VHF
  GPIO3, // UHF
  GPIO4, // ?
  GPIO5, // PA
  GPIO6, // RX
};

template <BK4819_GPIO_PIN PinNum> struct BK4819_Pin {
  static constexpr uint16_t pin_bit = 1 << static_cast<uint8_t>(PinNum);

  static void set() {
    gGpioOutState |= pin_bit;
    BK4819_WriteRegister(BK4819_REG_33, gGpioOutState);
  }

  static void clear() {
    gGpioOutState &= ~pin_bit;
    BK4819_WriteRegister(BK4819_REG_33, gGpioOutState);
  }

  static void toggle() {
    gGpioOutState ^= pin_bit;
    BK4819_WriteRegister(BK4819_REG_33, gGpioOutState);
  }

  static void write(bool state) {
    if (state)
      set();
    else
      clear();
  }
};

// ============================================================================
// Device Layer - K5 struct
// ============================================================================

struct K5 {
  static constexpr uint32_t GPIOA_BASE = GPIOA_BASE_ADDR;
  static constexpr uint32_t GPIOB_BASE = GPIOB_BASE_ADDR;
  static constexpr uint32_t GPIOC_BASE = GPIOC_BASE_ADDR;

  // === MCU GPIO (прямое управление) ===
  struct MCU {
    using PC3 = Pin<GPIOC_BASE, 3>;

    struct LED {
      using White = PC3;
      using Blue = Pin<GPIOA_BASE, 14>;

      static void init() {
        White::clear();
        Blue::clear();
      }
    };
  };

  // Строки — входы с pull-up, PA3..PA6
  using Row0 = Pin<GPIOA_BASE, 3>;
  using Row1 = Pin<GPIOA_BASE, 4>;
  using Row2 = Pin<GPIOA_BASE, 5>;
  using Row3 = Pin<GPIOA_BASE, 6>;

  // Колонки — выходы PA10..PA13
  using Col0 = Pin<GPIOA_BASE, 10>;
  using Col1 = Pin<GPIOA_BASE, 11>;
  using Col2 = Pin<GPIOA_BASE, 12>;
  using Col3 = Pin<GPIOA_BASE, 13>;

  struct Button {
    using PTT = Pin<GPIOC_BASE, 5>;

    static void init() {
      /* PTT::setInput();
      Menu::setInput();
      Up::setInput();
      Down::setInput(); */
    }

    static bool isPTTPressed() { return !PTT::read(); } // Active low
  };

  struct Keyboard {
    static void init() {
      return;
      // Настроить строки как input с pull-up
      Row0::setInput();
      Row1::setInput();
      Row2::setInput();
      Row3::setInput();

      // Настроить колонки как output и установить HIGH
      Col0::setOutput();
      Col1::setOutput();
      Col2::setOutput();
      Col3::setOutput();

      Col0::set();
      Col1::set();
      Col2::set();
      Col3::set();
    }

    static int scan() {
      // Проще — явно по каждому
      bool hit[4][4] = {};

      // 4 колонки
      for (int col = 0; col < 4; ++col) {
        // Перед каждой итерацией устанавливай все HIGH
        Col0::set();
        Col1::set();
        Col2::set();
        Col3::set();

        // Обнуляй одну нужную колонку
        switch (col) {
        case 0:
          Col0::clear();
          break;
        case 1:
          Col1::clear();
          break;
        case 2:
          Col2::clear();
          break;
        case 3:
          Col3::clear();
          break;
        }

        // Проверяй строки
        if (!Row0::read())
          return 0 * 4 + col;
        if (!Row1::read())
          return 1 * 4 + col;
        if (!Row2::read())
          return 2 * 4 + col;
        if (!Row3::read())
          return 3 * 4 + col;
      }

      return -1; // Нет нажатий
    }
  };

  enum class Key {
    None = -1,
    M = 0,
    Up = 1,
    Down = 2,
    Exit = 3,
    Key_1 = 4,
    Key_2 = 5,
    Key_3 = 6,
    Key_4 = 7,
    Key_5 = 8,
    Key_6 = 9,
    Key_7 = 10,
    Key_8 = 11,
    Key_9 = 12,
    Key_A = 13,
    Key_B = 14,
    Key_C = 15
  };

  struct Timer {
    static uint32_t millis() { return GetUptimeMs(); }
  };

  struct KeyboardController {
    Key lastKey = Key::None;
    uint32_t lastDebounceTime = 0;
    static constexpr uint32_t debounceDelay = 1; // ms

    void update() {
      int rawKey = K5::Keyboard::scan();
      Key key = rawKey == -1 ? Key::None : static_cast<Key>(rawKey);

      uint32_t now = K5::Timer::millis();
      printf("key %d\n", key);
      if (key != lastKey) {
        lastDebounceTime = now;
      }

      if ((now - lastDebounceTime) > debounceDelay) {
        if (key != lastKey) {
          lastKey = key;
          onKeyPress(key);
        }
      }
    }

    void onKeyPress(Key key) {
      // Тут можно обрабатывать событие клавиши
      printf("Key pressed: %d\n", static_cast<int>(key));
    }
  };

  struct Serial {
    static void printf(const char *pattern, ...) {
      char text[128];
      va_list va;
      va_start(va, pattern);
      UART_Send(text, vsnprintf(text, sizeof(text), pattern, va));
      va_end(va);
    }
  };

  // === BK4819 Chip (управление через SPI) ===
  struct BK4819 {
    struct LED {
      using Green = BK4819_Pin<BK4819_GPIO_PIN::GPIO0>;
      using Red = BK4819_Pin<BK4819_GPIO_PIN::GPIO1>;
    };

    static void init() {}
  };

  // === Application Layer ===
  struct Flashlight {
    static void on() { MCU::LED::White::set(); }
    static void off() { MCU::LED::White::clear(); }
    static void toggle() { MCU::LED::White::toggle(); }
  };
  struct LED {
    enum class Color { Off, Red, Green, Blue, Yellow, Cyan, Magenta, White };
    static void set(Color color) {
      switch (color) {
      case Color::Off:
        BK4819::LED::Green::clear();
        BK4819::LED::Red::clear();
        MCU::LED::Blue::clear();
        break;
      case Color::Red:
        BK4819::LED::Green::clear();
        BK4819::LED::Red::set();
        MCU::LED::Blue::clear();
        break;
      case Color::Green:
        BK4819::LED::Green::set();
        BK4819::LED::Red::clear();
        MCU::LED::Blue::clear();
        break;
      case Color::Blue:
        BK4819::LED::Green::clear();
        BK4819::LED::Red::clear();
        MCU::LED::Blue::set();
        break;
      case Color::Yellow:
        BK4819::LED::Green::set();
        BK4819::LED::Red::set();
        MCU::LED::Blue::clear();
        break;
      case Color::Cyan:
        BK4819::LED::Green::set();
        BK4819::LED::Red::clear();
        MCU::LED::Blue::set();
        break;
      case Color::Magenta:
        BK4819::LED::Green::clear();
        BK4819::LED::Red::set();
        MCU::LED::Blue::set();
        break;
      case Color::White:
        BK4819::LED::Green::set();
        BK4819::LED::Red::set();
        MCU::LED::Blue::set();
        break;
      }
    }
  };

  struct StatusLED {
    enum class Status { Idle, Scanning, Transmitting, Receiving, Error };

    static void set(Status status) {
      using LED = K5::LED;
      switch (status) {
      case Status::Idle:
        LED::set(LED::Color::Green);
        break;
      case Status::Scanning:
        LED::set(LED::Color::Blue);
        break;
      case Status::Transmitting:
        LED::set(LED::Color::Red);
        break;
      case Status::Receiving:
        LED::set(LED::Color::Cyan);
        break;
      case Status::Error:
        LED::set(LED::Color::Magenta);
        break;
      }
    }
  };

  static void init() {
    MCU::LED::init();
    BK4819::init();
  }
};
