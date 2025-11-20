#pragma once
#include "../inc/dp32g030/gpio.h"
#include "bk4819-regs.h" // Для BK4819_REG_33
#include "bk4819.h"
#include "gpio.h"
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

  enum class Key {
    None = -1,
    Key_0 = 0,
    Key_1 = 1,
    Key_2 = 2,
    Key_3 = 3,
    Key_4 = 4,
    Key_5 = 5,
    Key_6 = 6,
    Key_7 = 7,
    Key_8 = 8,
    Key_9 = 9,
    Menu = 10,
    Up = 11,
    Down = 12,
    Exit = 13,
    Star = 14,
    F = 15,
    SIDE1 = 16,
    SIDE2 = 17,
    PTT = 18,
    Invalid = 0xFF
  };

  struct Keyboard {
    // КОЛОНКИ — входы с pull-up PA0..PA3
    using Col0 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_0>; // PA0
    using Col1 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_1>; // PA1
    using Col2 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_2>; // PA2
    using Col3 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_3>; // PA3

    // СТРОКИ — выходы PA10..PA13
    using Row0 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_4>; // PA10
    using Row1 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_5>; // PA11
    using Row2 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_6>; // PA12
    using Row3 = Pin<GPIOA_BASE, GPIOA_PIN_KEYBOARD_7>; // PA13

    static void init() {
      // Колонки — входы с pull-up
      Col0::setInput();
      Col1::setInput();
      Col2::setInput();
      Col3::setInput();

      // Строки — выходы
      Row0::setOutput();
      Row1::setOutput();
      Row2::setOutput();
      Row3::setOutput();

      // Все строки в HIGH (неактивны)
      Row0::set();
      Row1::set();
      Row2::set();
      Row3::set();
    }

    static uint16_t readStableGpioData() {
      uint16_t reg = 0, reg2 = 0;
      for (int i = 0; i < 3; i++) {
        for (volatile int j = 0; j < 10; j++)
          ;
        reg2 = GPIOA->DATA;
        if (reg != reg2) {
          reg = reg2;
          i = 0;
        }
      }
      return reg;
    }

    static void resetKeyboardRow(uint8_t row) {
      // Сначала все строки в HIGH
      Row0::set();
      Row1::set();
      Row2::set();
      Row3::set();

      // Активировать нужную строку (LOW)
      switch (row) {
      case 0: /* Zero row - боковые кнопки, ничего не активируем */
        break;
      case 1:
        Row0::clear();
        break; // First row
      case 2:
        Row1::clear();
        break; // Second row
      case 3:
        Row2::clear();
        break; // Third row
      case 4:
        Row3::clear();
        break; // Fourth row
      }

      // Задержка для стабилизации
      for (volatile int i = 0; i < 10; i++)
        ;
    }

    static int scan() {
      // Правильная карта согласно keyboard.c
      static const int8_t keyMap[5][4] = {
          {16, 17, 17, 17}, // Zero row: SIDE1, SIDE2, SIDE2, SIDE2
          {10, 1, 4, 7},    // First row: MENU, 1, 4, 7
          {11, 2, 5, 8},    // Second row: UP, 2, 5, 8
          {12, 3, 6, 9},    // Third row: DOWN, 3, 6, 9
          {13, 14, 0, 15}   // Fourth row: EXIT, *, 0, F
      };

      for (uint8_t row = 0; row < 5; row++) {
        resetKeyboardRow(row);
        uint16_t reg = readStableGpioData();

        // Проверяем колонки (активны при LOW)
        if (!(reg & (1 << GPIOA_PIN_KEYBOARD_0))) {
          if (row == 0 && 0 > 1)
            continue; // игнорируем дубли
          return keyMap[row][0];
        }
        if (!(reg & (1 << GPIOA_PIN_KEYBOARD_1))) {
          if (row == 0 && 1 > 1)
            continue;
          return keyMap[row][1];
        }
        if (!(reg & (1 << GPIOA_PIN_KEYBOARD_2))) {
          if (row == 0 && 2 > 1)
            continue;
          return keyMap[row][2];
        }
        if (!(reg & (1 << GPIOA_PIN_KEYBOARD_3))) {
          if (row == 0 && 3 > 1)
            continue;
          return keyMap[row][3];
        }
      }

      return -1;
    }
  };

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

  enum class KeyEvent {
    None,
    Pressed,
    Released,
    LongPressed,     // 500ms
    LongPressRepeat, // каждые 100ms
    DoubleClick      // в течение 300ms
  };

  struct KeyboardController {
    Key lastKey = Key::None;
    Key currentKey = Key::None;
    Key lastReleasedKey =
        Key::None; // ✅ Запоминаем, какая клавиша была отпущена
    uint32_t lastDebounceTime = 0;
    uint32_t keyPressTime = 0;
    uint32_t lastReleaseTime = 0;
    uint32_t longPressRepeatTime = 0;

    static constexpr uint32_t debounceDelay = 50;
    static constexpr uint32_t longPressDelay = 500;
    static constexpr uint32_t longPressRepeat = 100;
    static constexpr uint32_t doubleClickWindow = 300;

    bool isLongPressed = false;

    void update() {
      int rawKey = K5::Keyboard::scan();
      Key key = rawKey == -1 ? Key::None : static_cast<Key>(rawKey);
      uint32_t now = K5::Timer::millis();

      // Дебаунсинг
      if (key != currentKey) {
        currentKey = key;
        lastDebounceTime = now;
      }

      if ((now - lastDebounceTime) > debounceDelay) {
        // Клавиша нажата
        if (currentKey != Key::None && lastKey == Key::None) {
          keyPressTime = now;
          isLongPressed = false;
          lastKey = currentKey;
          onKeyEvent(currentKey, KeyEvent::Pressed);

          // ✅ Double-click только если та же клавиша нажата повторно
          if (currentKey == lastReleasedKey &&
              (now - lastReleaseTime) < doubleClickWindow) {
            onKeyEvent(currentKey, KeyEvent::DoubleClick);
          }
        }
        // Клавиша удерживается
        else if (currentKey != Key::None && currentKey == lastKey) {
          uint32_t holdTime = now - keyPressTime;

          // Первое долгое нажатие
          if (holdTime >= longPressDelay && !isLongPressed) {
            isLongPressed = true;
            longPressRepeatTime = now;
            onKeyEvent(currentKey, KeyEvent::LongPressed);
          }
          // Повторяющееся долгое нажатие (авто-повтор)
          else if (isLongPressed &&
                   (now - longPressRepeatTime) >= longPressRepeat) {
            longPressRepeatTime = now;
            onKeyEvent(currentKey, KeyEvent::LongPressRepeat);
          }
        }
        // Клавиша отпущена
        else if (currentKey == Key::None && lastKey != Key::None) {
          lastReleasedKey = lastKey; // ✅ Сохраняем, какую клавишу отпустили
          lastReleaseTime = now;
          onKeyEvent(lastKey, KeyEvent::Released);
          lastKey = Key::None;
        }
      }
    }

    void onKeyEvent(Key key, KeyEvent event) {
      const char *eventNames[] = {
          "None",        "Pressed",         "Released",
          "LongPressed", "LongPressRepeat", "DoubleClick"};
      Log("Key %d: %s", static_cast<int>(key),
          eventNames[static_cast<int>(event)]);
    }
  };

  struct Timer {
    static uint32_t millis() { return GetUptimeMs(); }
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
