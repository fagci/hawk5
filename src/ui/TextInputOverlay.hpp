#pragma once
#include "../driver/keyboard.h"
#include "../scheduler.h"
#include "../ui/graphics.h"
#include <cstdint>
#include <cstring>
#include <functional>

// ============================================================================
// TEXT INPUT OVERLAY (for names, codes, etc.)
// ============================================================================

class TextInputOverlay {
public:
  TextInputOverlay() = default;

  // ========================================================================
  // PUBLIC API
  // ========================================================================

  // Открыть overlay для ввода текста
  void open(const char *initialText, uint8_t maxLength, const char *title,
            std::function<void(const char *)> callback) {
    maxLength_ = (maxLength > MAX_TEXT_LENGTH) ? MAX_TEXT_LENGTH : maxLength;
    strncpy(buffer_, initialText ? initialText : "", maxLength_);
    buffer_[maxLength_] = '\0';
    cursorPos_ = strlen(buffer_);

    strncpy(title_, title ? title : "Enter Text", sizeof(title_) - 1);
    title_[sizeof(title_) - 1] = '\0';

    callback_ = callback;
    active_ = true;
    blinkState_ = true;
    lastBlinkTime_ = Now();

    // Initialize charset
    currentCharset_ = Charset::UPPERCASE;
  }

  void close() { active_ = false; }

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

    // Draw background
    FillRect(0, 16, LCD_WIDTH, LCD_HEIGHT - 16, C_CLEAR);
    DrawRect(2, 18, LCD_WIDTH - 4, LCD_HEIGHT - 20, C_FILL);

    // Draw title
    PrintSmall(4, 14, title_);

    // Draw input line
    DrawHLine(4, 30, LCD_WIDTH - 8, C_FILL);

    // Draw text with cursor
    char displayText[MAX_TEXT_LENGTH + 2];
    strncpy(displayText, buffer_, MAX_TEXT_LENGTH);
    displayText[MAX_TEXT_LENGTH] = '\0';

    PrintMedium(6, 29, "%s", displayText);

    // Draw cursor
    if (blinkState_ && cursorPos_ <= strlen(buffer_)) {
      uint8_t cursorX = 6 + cursorPos_ * 6; // 6px per char for medium font
      DrawVLine(cursorX, 34, 8, C_FILL);
    }

    // Draw charset indicator
    const char *charsetName = getCharsetName();
    PrintSmall(4, 46, "Mode: %s", charsetName);

    // Draw char selector
    if (selectingChar_) {
      renderCharSelector();
    } else {
      // Draw hints
      PrintSmall(4, 54, "* = Space  # = Mode");
      PrintSmall(4, 60, "0-9 = Chars");
    }
  }

private:
  // ========================================================================
  // CONSTANTS
  // ========================================================================

  static constexpr uint8_t MAX_TEXT_LENGTH = 16;

  enum class Charset : uint8_t { UPPERCASE, LOWERCASE, NUMBERS, SYMBOLS };

  // Character maps for T9-style input
  static constexpr const char *CHARSET_UPPER[] = {
      " ",     // 0
      ".,?!1", // 1
      "ABC2",  // 2
      "DEF3",  // 3
      "GHI4",  // 4
      "JKL5",  // 5
      "MNO6",  // 6
      "PQRS7", // 7
      "TUV8",  // 8
      "WXYZ9"  // 9
  };

  static constexpr const char *CHARSET_LOWER[] = {
      " ",     // 0
      ".,?!1", // 1
      "abc2",  // 2
      "def3",  // 3
      "ghi4",  // 4
      "jkl5",  // 5
      "mno6",  // 6
      "pqrs7", // 7
      "tuv8",  // 8
      "wxyz9"  // 9
  };

  static constexpr const char *CHARSET_NUMBERS[] = {"0", "1", "2", "3", "4",
                                                    "5", "6", "7", "8", "9"};

  static constexpr const char *CHARSET_SYMBOLS[] = {
      " ",      // 0
      "!@#1",   // 1
      "ABC2",   // 2
      "$%^3",   // 3
      "&*()4",  // 4
      "-_=+5",  // 5
      "[]{}6",  // 6
      ";:'\"7", // 7
      "<>,./8", // 8
      "?\\|`9"  // 9
  };

  // ========================================================================
  // STATE
  // ========================================================================

  bool active_ = false;
  char buffer_[MAX_TEXT_LENGTH + 1] = "";
  uint8_t cursorPos_ = 0;
  uint8_t maxLength_ = MAX_TEXT_LENGTH;
  char title_[20] = "Enter Text";

  std::function<void(const char *)> callback_;

  // Charset state
  Charset currentCharset_ = Charset::UPPERCASE;

  // Character selection state (T9-style)
  bool selectingChar_ = false;
  uint8_t lastKey_ = 0xFF;
  uint8_t charIndex_ = 0;
  uint32_t lastKeyTime_ = 0;
  static constexpr uint32_t KEY_TIMEOUT_MS = 1000;

  // Blink state
  bool blinkState_ = false;
  uint32_t lastBlinkTime_ = 0;

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
      return handleCharKey(key);

    case KEY_STAR:
      return handleSpaceKey();

    case KEY_F:
      return handleModeKey();

    case KEY_EXIT:
      return handleBackspaceKey();

    case KEY_MENU:
      return handleConfirmKey();

    case KEY_UP:
      return handleCursorLeft();

    case KEY_DOWN:
      return handleCursorRight();

    default:
      break;
    }

    return false;
  }

  bool handleCharKey(KEY_Code_t key) {
    uint8_t keyNum = key - KEY_0;
    uint32_t now = Now();

    // Check if same key pressed within timeout
    if (lastKey_ == keyNum && (now - lastKeyTime_) < KEY_TIMEOUT_MS) {
      // Cycle through characters for this key
      const char *charset = getCurrentCharset(keyNum);
      uint8_t charsetLen = strlen(charset);

      if (charsetLen > 0) {
        charIndex_ = (charIndex_ + 1) % charsetLen;

        // Replace character at cursor
        if (cursorPos_ > 0 && cursorPos_ <= strlen(buffer_)) {
          buffer_[cursorPos_ - 1] = charset[charIndex_];
        }
      }
    } else {
      // New key or timeout - insert first character
      const char *charset = getCurrentCharset(keyNum);

      if (strlen(charset) > 0 && cursorPos_ < maxLength_) {
        charIndex_ = 0;

        // Insert character
        if (cursorPos_ < strlen(buffer_)) {
          // Insert in middle - shift right
          memmove(&buffer_[cursorPos_ + 1], &buffer_[cursorPos_],
                  strlen(buffer_) - cursorPos_ + 1);
        }

        buffer_[cursorPos_] = charset[charIndex_];
        cursorPos_++;
        buffer_[cursorPos_] = '\0';
      }
    }

    lastKey_ = keyNum;
    lastKeyTime_ = now;

    return true;
  }

  bool handleSpaceKey() {
    if (cursorPos_ >= maxLength_)
      return true;

    // Insert space
    if (cursorPos_ < strlen(buffer_)) {
      memmove(&buffer_[cursorPos_ + 1], &buffer_[cursorPos_],
              strlen(buffer_) - cursorPos_ + 1);
    }

    buffer_[cursorPos_] = ' ';
    cursorPos_++;
    buffer_[cursorPos_] = '\0';

    lastKey_ = 0xFF; // Reset T9 state

    return true;
  }

  bool handleModeKey() {
    // Cycle through charsets
    switch (currentCharset_) {
    case Charset::UPPERCASE:
      currentCharset_ = Charset::LOWERCASE;
      break;
    case Charset::LOWERCASE:
      currentCharset_ = Charset::NUMBERS;
      break;
    case Charset::NUMBERS:
      currentCharset_ = Charset::SYMBOLS;
      break;
    case Charset::SYMBOLS:
      currentCharset_ = Charset::UPPERCASE;
      break;
    }

    lastKey_ = 0xFF; // Reset T9 state

    return true;
  }

  bool handleBackspaceKey() {
    if (cursorPos_ == 0) {
      // Close overlay
      close();
      return true;
    }

    // Delete character at cursor
    if (cursorPos_ > 0 && cursorPos_ <= strlen(buffer_)) {
      memmove(&buffer_[cursorPos_ - 1], &buffer_[cursorPos_],
              strlen(buffer_) - cursorPos_ + 1);
      cursorPos_--;
    }

    lastKey_ = 0xFF; // Reset T9 state

    return true;
  }

  bool handleConfirmKey() {
    // Trim trailing spaces
    while (strlen(buffer_) > 0 && buffer_[strlen(buffer_) - 1] == ' ') {
      buffer_[strlen(buffer_) - 1] = '\0';
    }

    if (callback_) {
      callback_(buffer_);
    }

    close();
    return true;
  }

  bool handleCursorLeft() {
    if (cursorPos_ > 0) {
      cursorPos_--;
      lastKey_ = 0xFF; // Reset T9 state
    }
    return true;
  }

  bool handleCursorRight() {
    if (cursorPos_ < strlen(buffer_)) {
      cursorPos_++;
      lastKey_ = 0xFF; // Reset T9 state
    }
    return true;
  }

  // ========================================================================
  // HELPERS
  // ========================================================================

  const char *getCurrentCharset(uint8_t keyNum) const {
    if (keyNum > 9)
      return "";

    switch (currentCharset_) {
    case Charset::UPPERCASE:
      return CHARSET_UPPER[keyNum];
    case Charset::LOWERCASE:
      return CHARSET_LOWER[keyNum];
    case Charset::NUMBERS:
      return CHARSET_NUMBERS[keyNum];
    case Charset::SYMBOLS:
      return CHARSET_SYMBOLS[keyNum];
    default:
      return "";
    }
  }

  const char *getCharsetName() const {
    switch (currentCharset_) {
    case Charset::UPPERCASE:
      return "ABC";
    case Charset::LOWERCASE:
      return "abc";
    case Charset::NUMBERS:
      return "123";
    case Charset::SYMBOLS:
      return "#@!";
    default:
      return "?";
    }
  }

  void renderCharSelector() {
    // Draw available chars for current key
    if (lastKey_ <= 9) {
      const char *charset = getCurrentCharset(lastKey_);
      PrintSmall(4, 54, "Key %d: %s", lastKey_, charset);

      // Highlight current char
      if (charIndex_ < strlen(charset)) {
        char highlighted[2] = {charset[charIndex_], '\0'};
        PrintMediumEx(4 + 40 + charIndex_ * 8, 54, POS_L, C_INVERT, "%s",
                      highlighted);
      }
    }
  }
};
