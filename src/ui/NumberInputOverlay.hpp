#pragma once
#include <cstdint>
#include <cstring>
#include <functional>

#include "../driver/keyboard.h"
#include "../scheduler.h"
#include "graphics.h"

// ============================================================================
// INPUT UNITS
// ============================================================================

enum class InputUnit : uint8_t {
  RAW,    // Без единиц измерения
  Hz,     // Герцы
  KHz,    // Килогерцы
  MHz,    // Мегагерцы
  VOLTS,  // Вольты
  DBM,    // dBm
  PERCENT // Проценты
};

// ============================================================================
// NUMBER INPUT OVERLAY
// ============================================================================

class NumberInputOverlay {
public:
  NumberInputOverlay() = default;

  // ========================================================================
  // PUBLIC API
  // ========================================================================

  // Открыть overlay для ввода одного значения
  void open(uint32_t initialValue, uint32_t min, uint32_t max, InputUnit unit,
            std::function<void(uint32_t)> callback) {
    isRange_ = false;
    value1_ = initialValue;
    value2_ = 0;
    min_ = min;
    max_ = max;
    unit_ = unit;
    callback_ = callback;
    rangeCallback_ = nullptr;

    setup();
    active_ = true;
  }

  // Открыть overlay для ввода диапазона
  void openRange(uint32_t initialStart, uint32_t initialEnd, uint32_t min,
                 uint32_t max, InputUnit unit,
                 std::function<void(uint32_t, uint32_t)> callback) {
    isRange_ = true;
    value1_ = initialStart;
    value2_ = initialEnd;
    min_ = min;
    max_ = max;
    unit_ = unit;
    callback_ = nullptr;
    rangeCallback_ = callback;

    setup();
    active_ = true;
  }

  void close() {
    active_ = false;
    reset();
  }

  bool isActive() const { return active_; }

  // ========================================================================
  // EVENT HANDLERS
  // ========================================================================

  bool handleKey(KEY_Code_t key, Key_State_t state) {
    if (!active_)
      return false;

    // Long press EXIT = cancel
    if (state == KEY_LONG_PRESSED && key == KEY_EXIT) {
      close();
      return true;
    }

    if (state == KEY_RELEASED) {
      return handleKeyReleased(key);
    }

    return false;
  }

  void update() {
    if (!active_)
      return;

    // Blink cursor every 500ms
    if (Now() - lastBlinkTime_ >= 500) {
      blinkState_ = !blinkState_;
      lastBlinkTime_ = Now();
    }
  }

  void render() {
    if (!active_)
      return;

    UI_ClearScreen();
    renderInput();
    renderInfo();
    renderLimits();
  }

private:
  // ========================================================================
  // CONSTANTS
  // ========================================================================

  static constexpr uint8_t MAX_INPUT_LENGTH = 10;
  static constexpr uint8_t MAX_FRACTION_DIGITS = 6;
  static constexpr uint8_t BIG_DIGIT_HEIGHT = 11;
  static constexpr uint8_t BIG_DIGIT_WIDTH = 11;
  static constexpr uint8_t BASE_Y = 32;

  static constexpr uint32_t POW[8] = {1,     10,     100,     1000,
                                      10000, 100000, 1000000, 10000000};

  // ========================================================================
  // STATE
  // ========================================================================

  bool active_ = false;
  bool isRange_ = false;

  uint32_t value1_ = 0;
  uint32_t value2_ = 0;
  uint32_t min_ = 0;
  uint32_t max_ = UINT32_MAX;
  InputUnit unit_ = InputUnit::RAW;

  std::function<void(uint32_t)> callback_;
  std::function<void(uint32_t, uint32_t)> rangeCallback_;

  // Input buffer
  char inputBuffer_[MAX_INPUT_LENGTH + 1] = "";
  uint8_t cursorPos_ = 0;
  bool dotEntered_ = false;
  uint8_t fractionalDigits_ = 0;

  // Range input stage
  enum class InputStage {
    FIRST_VALUE,
    SECOND_VALUE
  } inputStage_ = InputStage::FIRST_VALUE;

  // Blink state
  bool blinkState_ = false;
  uint32_t lastBlinkTime_ = 0;

  // Config
  uint8_t maxDigits_ = MAX_INPUT_LENGTH;
  bool allowDot_ = false;
  uint32_t multiplier_ = 1;

  // ========================================================================
  // SETUP & RESET
  // ========================================================================

  void setup() {
    // Configure based on unit
    allowDot_ = (unit_ == InputUnit::MHz || unit_ == InputUnit::KHz ||
                 unit_ == InputUnit::VOLTS);

    switch (unit_) {
    case InputUnit::Hz:
      multiplier_ = 1;
      break;
    case InputUnit::KHz:
      multiplier_ = 100; // 1 kHz = 100 * 10Hz
      break;
    case InputUnit::MHz:
      multiplier_ = 100000; // 1 MHz = 100000 * 10Hz
      break;
    case InputUnit::VOLTS:
      multiplier_ = 100;
      break;
    default:
      multiplier_ = 1;
      break;
    }

    // Calculate max digits
    uint32_t maxDisplayValue = convertToDisplayValue(max_);
    maxDigits_ = 0;
    while (maxDisplayValue > 0) {
      maxDisplayValue /= 10;
      maxDigits_++;
    }
    if (maxDigits_ > MAX_INPUT_LENGTH - 1) {
      maxDigits_ = MAX_INPUT_LENGTH - 1;
    }

    // Fill from current value
    inputStage_ = InputStage::FIRST_VALUE;
    if (isRange_ && value1_ != 0) {
      inputStage_ = InputStage::SECOND_VALUE;
    }
    fillFromCurrentValue();
  }

  void reset() {
    cursorPos_ = 0;
    dotEntered_ = false;
    fractionalDigits_ = 0;
    memset(inputBuffer_, 0, sizeof(inputBuffer_));
    blinkState_ = false;
    inputStage_ = InputStage::FIRST_VALUE;
  }

  void resetInputBuffer() {
    cursorPos_ = 0;
    dotEntered_ = false;
    fractionalDigits_ = 0;
    memset(inputBuffer_, 0, sizeof(inputBuffer_));
  }

  // ========================================================================
  // VALUE CONVERSION
  // ========================================================================

  uint32_t convertFromDisplayValue() const {
    uint32_t integerPart = 0;
    uint32_t fractionalPart = 0;
    bool isFractional = false;
    uint8_t localFractionalDigits = 0;

    for (uint8_t i = 0; i < cursorPos_ && inputBuffer_[i] != '\0'; i++) {
      if (inputBuffer_[i] == '.') {
        isFractional = true;
      } else if (inputBuffer_[i] >= '0' && inputBuffer_[i] <= '9') {
        if (isFractional) {
          fractionalPart = fractionalPart * 10 + (inputBuffer_[i] - '0');
          localFractionalDigits++;
        } else {
          integerPart = integerPart * 10 + (inputBuffer_[i] - '0');
        }
      }
    }

    // Convert to native units
    if (unit_ != InputUnit::RAW) {
      uint32_t result = integerPart * multiplier_;
      if (localFractionalDigits > 0) {
        result += (fractionalPart * multiplier_) / POW[localFractionalDigits];
      }
      return result;
    }

    return integerPart;
  }

  uint32_t convertToDisplayValue(uint32_t nativeValue) const {
    switch (unit_) {
    case InputUnit::Hz:
      return nativeValue;
    case InputUnit::KHz:
      return nativeValue / 100;
    case InputUnit::MHz:
      return nativeValue / 100000;
    case InputUnit::VOLTS:
      return nativeValue / 100;
    default:
      return nativeValue;
    }
  }

  void fillFromCurrentValue() {
    uint32_t value =
        (inputStage_ == InputStage::FIRST_VALUE) ? value1_ : value2_;

    if (value == 0) {
      resetInputBuffer();
      return;
    }

    uint32_t integerPart = 0;
    uint32_t fractionalPart = 0;
    uint8_t displayFractionalDigits = 0;

    // Convert from native units to display units
    switch (unit_) {
    case InputUnit::Hz:
      integerPart = value;
      fractionalPart = 0;
      displayFractionalDigits = 0;
      break;
    case InputUnit::KHz:
      integerPart = value / 100;
      fractionalPart = value % 100;
      displayFractionalDigits = 2;
      break;
    case InputUnit::MHz:
      integerPart = value / 100000;
      fractionalPart = value % 100000;
      displayFractionalDigits = 5;
      break;
    case InputUnit::VOLTS:
      integerPart = value / 100;
      fractionalPart = value % 100;
      displayFractionalDigits = 2;
      break;
    default:
      integerPart = value;
      break;
    }

    // Remove trailing zeros
    if (displayFractionalDigits > 0 && fractionalPart > 0) {
      while (fractionalPart % 10 == 0 && displayFractionalDigits > 0) {
        fractionalPart /= 10;
        displayFractionalDigits--;
      }
    }

    if (displayFractionalDigits > 0) {
      snprintf(inputBuffer_, sizeof(inputBuffer_), "%lu.%0*lu", integerPart,
               displayFractionalDigits, fractionalPart);
    } else {
      snprintf(inputBuffer_, sizeof(inputBuffer_), "%lu", integerPart);
    }

    cursorPos_ = strlen(inputBuffer_);
    dotEntered_ = strchr(inputBuffer_, '.') != nullptr;
    fractionalDigits_ = displayFractionalDigits;
  }

  // ========================================================================
  // KEY HANDLING
  // ========================================================================

  bool handleKeyReleased(KEY_Code_t key) {
    switch (key) {
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
      return handleDigitKey(key);

    case KEY_STAR:
      return handleDotKey();

    case KEY_EXIT:
      return handleExitKey();

    case KEY_MENU:
    case KEY_F:
    case KEY_PTT:
      return handleConfirmKey();

    default:
      break;
    }

    return false;
  }

  bool handleDigitKey(KEY_Code_t key) {
    if (cursorPos_ >= MAX_INPUT_LENGTH - 1)
      return true;
    if (dotEntered_ && fractionalDigits_ >= MAX_FRACTION_DIGITS)
      return true;

    // Check if adding new digit would exceed max value
    uint32_t currentValue = convertFromDisplayValue();
    uint32_t newDigit = key - KEY_0;
    uint32_t newValue = currentValue * 10 + newDigit * multiplier_;

    if (!dotEntered_ && allowDot_ && newValue > max_ && cursorPos_ > 0) {
      // Auto-insert decimal point
      inputBuffer_[cursorPos_++] = '.';
      inputBuffer_[cursorPos_] = '\0';
      dotEntered_ = true;
    }

    // Add digit
    inputBuffer_[cursorPos_++] = '0' + (key - KEY_0);
    inputBuffer_[cursorPos_] = '\0';

    if (dotEntered_)
      fractionalDigits_++;

    // Update current value
    if (inputStage_ == InputStage::FIRST_VALUE) {
      value1_ = convertFromDisplayValue();
    } else {
      value2_ = convertFromDisplayValue();
    }

    return true;
  }

  bool handleDotKey() {
    if (!allowDot_ || dotEntered_ || cursorPos_ == 0)
      return true;
    if (cursorPos_ >= MAX_INPUT_LENGTH - 1)
      return true;

    inputBuffer_[cursorPos_++] = '.';
    inputBuffer_[cursorPos_] = '\0';
    dotEntered_ = true;

    return true;
  }

  bool handleExitKey() {
    if (cursorPos_ == 0) {
      if (isRange_ && inputStage_ == InputStage::SECOND_VALUE) {
        // Go back to first value
        inputStage_ = InputStage::FIRST_VALUE;
        resetInputBuffer();
        fillFromCurrentValue();
        return true;
      }
      // Close overlay
      close();
      return true;
    }

    // Backspace
    if (cursorPos_ > 0) {
      if (inputBuffer_[cursorPos_ - 1] == '.') {
        dotEntered_ = false;
      } else if (dotEntered_ && fractionalDigits_ > 0) {
        fractionalDigits_--;
      }

      inputBuffer_[--cursorPos_] = '\0';

      if (inputStage_ == InputStage::FIRST_VALUE) {
        value1_ = convertFromDisplayValue();
      } else {
        value2_ = convertFromDisplayValue();
      }
    }

    return true;
  }

  bool handleConfirmKey() {
    if (inputStage_ == InputStage::FIRST_VALUE) {
      value1_ = convertFromDisplayValue();

      if (isRange_) {
        // Move to second value
        inputStage_ = InputStage::SECOND_VALUE;
        resetInputBuffer();
        return true;
      }

      // Validate and confirm single value
      if (value1_ >= min_ && value1_ <= max_) {
        if (callback_) {
          callback_(value1_);
        }
        close();
      }
      return true;
    }

    // Second value (range)
    value2_ = convertFromDisplayValue();

    // Swap if needed
    if (value2_ < value1_) {
      uint32_t temp = value1_;
      value1_ = value2_;
      value2_ = temp;
    }

    if (rangeCallback_) {
      rangeCallback_(value1_, value2_);
    }
    close();

    return true;
  }

  // ========================================================================
  // RENDERING
  // ========================================================================

  void renderInput() {
    char displayStr[MAX_INPUT_LENGTH + 3] = "";
    if (inputBuffer_[0] != '\0') {
      strncpy(displayStr, inputBuffer_, MAX_INPUT_LENGTH);
    }

    const char *unitSuffix = getUnitSuffix();

    // Render big digits
    PrintBiggestDigitsEx(LCD_WIDTH - 3, BASE_Y, POS_R, C_FILL, "%s",
                         displayStr);
    PrintMediumEx(LCD_WIDTH - 3, BASE_Y + 8, POS_R, C_FILL, "%s", unitSuffix);

    // Render cursor
    if (blinkState_ &&
        cursorPos_ < maxDigits_ + (dotEntered_ ? MAX_FRACTION_DIGITS : 0)) {
      uint8_t cursorX =
          LCD_WIDTH - 1 - (strlen(displayStr) - cursorPos_) * BIG_DIGIT_WIDTH;
      FillRect(cursorX, BASE_Y - BIG_DIGIT_HEIGHT + 1, 1, BIG_DIGIT_HEIGHT,
               C_FILL);
    }
  }

  void renderInfo() {
    if (!isRange_)
      return;

    char rangeStr[32];
    if (inputStage_ == InputStage::FIRST_VALUE) {
      if (cursorPos_ > 0) {
        snprintf(rangeStr, sizeof(rangeStr), "Start: %lu",
                 convertFromDisplayValue());
      } else {
        snprintf(rangeStr, sizeof(rangeStr), "Start: %lu", value1_);
      }
    } else {
      snprintf(rangeStr, sizeof(rangeStr), "End: %lu-%lu", value1_,
               cursorPos_ > 0 ? convertFromDisplayValue() : value1_);
    }

    PrintSmall(0, 16, rangeStr);
  }

  void renderLimits() {
    if (min_ > 0) {
      renderLimit("Min", min_, 32);
    }
    if (max_ < UINT32_MAX) {
      renderLimit("Max", max_, 42);
    }
  }

  void renderLimit(const char *label, uint32_t value, uint8_t y) {
    switch (unit_) {
    case InputUnit::MHz:
      PrintSmall(0, y, "%s: %u.%05u MHz", label, value / 100000,
                 value % 100000);
      break;
    case InputUnit::KHz:
      PrintSmall(0, y, "%s: %u.%02u kHz", label, value / 100, value % 100);
      break;
    case InputUnit::Hz:
      PrintSmall(0, y, "%s: %u Hz", label, value);
      break;
    case InputUnit::VOLTS:
      PrintSmall(0, y, "%s: %u.%02u V", label, value / 100, value % 100);
      break;
    default:
      PrintSmall(0, y, "%s: %u", label, value);
      break;
    }
  }

  const char *getUnitSuffix() const {
    switch (unit_) {
    case InputUnit::Hz:
      return "Hz";
    case InputUnit::KHz:
      return "kHz";
    case InputUnit::MHz:
      return "MHz";
    case InputUnit::VOLTS:
      return "V";
    case InputUnit::DBM:
      return "dBm";
    case InputUnit::PERCENT:
      return "%";
    default:
      return "";
    }
  }
};
