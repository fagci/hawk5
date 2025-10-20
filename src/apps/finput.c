#include "finput.h"
#include "../driver/st7565.h"
#include "../driver/uart.h"
#include "../scheduler.h"
#include "../ui/graphics.h"
#include "apps.h"
#include <string.h>

// Константы для преобразования частот
#define HZ 1       // 1 * 10Hz
#define KHZ 100    // 100 * 10Hz = 1kHz
#define MHZ 100000 // 100000 * 10Hz = 1MHz

static uint32_t POW[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

static const uint8_t BIG_DIGIT_HEIGHT = 11;
static const uint8_t BIG_DIGIT_WIDTH = 11;
static const uint8_t MAX_FRACTION_DIGITS = 6;

typedef enum { INPUT_FIRST_VALUE, INPUT_SECOND_VALUE } InputStage;

typedef struct {
  uint32_t min;
  uint32_t max;
  InputUnit unit;
  bool is_range;
  uint8_t max_digits;
  bool allow_dot;
  uint32_t multiplier;
} InputConfig;

uint32_t gFInputValue1;
uint32_t gFInputValue2;
void (*gFInputCallback)(uint32_t value1, uint32_t value2);

#define MAX_INPUT_LENGTH 10

static char inputBuffer[MAX_INPUT_LENGTH] = "";
static uint8_t cursorPos = 0;
static uint8_t blinkState = 0;
static uint32_t lastUpdate;
static bool dotEntered = false;
static uint8_t fractionalDigits = 0;
static InputStage inputStage = INPUT_FIRST_VALUE;

static InputConfig currentConfig = {.min = 0,
                                    .max = UINT32_MAX,
                                    .unit = UNIT_RAW,
                                    .is_range = false,
                                    .max_digits = MAX_INPUT_LENGTH,
                                    .allow_dot = false,
                                    .multiplier = 1};

/* static uint32_t convertFromDisplayValue() {
  uint32_t integerPart = 0;
  uint32_t fractionalPart = 0;
  bool isFractional = false;
  fractionalDigits = 0;

  for (uint8_t i = 0; i < cursorPos && inputBuffer[i] != '\0'; i++) {
    if (inputBuffer[i] == '.') {
      isFractional = true;
    } else if (inputBuffer[i] >= '0' && inputBuffer[i] <= '9') {
      if (isFractional) {
        fractionalPart = fractionalPart * 10 + (inputBuffer[i] - '0');
        fractionalDigits++;
      } else {
        integerPart = integerPart * 10 + (inputBuffer[i] - '0');
      }
    }
  }

  if (currentConfig.unit == UNIT_HZ || currentConfig.unit == UNIT_KHZ ||
      currentConfig.unit == UNIT_MHZ || currentConfig.unit == UNIT_VOLTS) {
    return (integerPart * currentConfig.multiplier) +
           (fractionalPart * currentConfig.multiplier / POW[fractionalDigits]);
  }

  switch (currentConfig.unit) {
  default:
    return integerPart;
  }
} */

static uint32_t convertFromDisplayValue() {
  uint32_t integerPart = 0;
  uint32_t fractionalPart = 0;
  bool isFractional = false;
  uint8_t localFractionalDigits = 0;

  for (uint8_t i = 0; i < cursorPos && inputBuffer[i] != '\0'; i++) {
    if (inputBuffer[i] == '.') {
      isFractional = true;
    } else if (inputBuffer[i] >= '0' && inputBuffer[i] <= '9') {
      if (isFractional) {
        fractionalPart = fractionalPart * 10 + (inputBuffer[i] - '0');
        localFractionalDigits++;
      } else {
        integerPart = integerPart * 10 + (inputBuffer[i] - '0');
      }
    }
  }

  // Конвертируем в нативные единицы (десятки Герц)
  if (currentConfig.unit == UNIT_HZ || currentConfig.unit == UNIT_KHZ ||
      currentConfig.unit == UNIT_MHZ || currentConfig.unit == UNIT_VOLTS) {
    uint32_t result = integerPart * currentConfig.multiplier;
    if (localFractionalDigits > 0) {
      result += (fractionalPart * currentConfig.multiplier) /
                POW[localFractionalDigits];
    }
    return result;
  }

  return integerPart;
}

static void resetInputBuffer() {
  cursorPos = 0;
  dotEntered = false;
  fractionalDigits = 0;
  memset(inputBuffer, 0, MAX_INPUT_LENGTH);
}

static void reset() {
  inputStage = INPUT_FIRST_VALUE;
  resetInputBuffer();
  blinkState = 0;
}

/* static void fillFromCurrentValue() {
  uint32_t value =
      (inputStage == INPUT_FIRST_VALUE) ? gFInputValue1 : gFInputValue2;

  // Skip filling if value is 0 to prevent displaying initial "0"
  if (value == 0) {
    resetInputBuffer();
    return;
  }

  uint32_t integerPart = 0;
  uint32_t fractionalPart = 0;
  fractionalDigits = 0;

  switch (currentConfig.unit) {
  case UNIT_HZ:
    integerPart = value / (HZ);
    fractionalPart = value % (HZ);
    fractionalDigits = 0;
    break;
  case UNIT_KHZ:
    integerPart = value / (KHZ);
    fractionalPart = value % (KHZ);
    fractionalDigits = 3;
    break;
  case UNIT_MHZ:
    integerPart = value / (MHZ);
    fractionalPart = value % (MHZ);
    fractionalDigits = 5;
    break;
  case UNIT_VOLTS:
    integerPart = value / 100;
    fractionalPart = value % 100;
    fractionalDigits = 2;
    break;
  default:
    integerPart = value;
    break;
  }

  if (fractionalDigits > 0 && fractionalPart > 0) {
    while (fractionalPart % 10 == 0 && fractionalDigits > 0) {
      fractionalPart /= 10;
      fractionalDigits--;
    }
    snprintf(inputBuffer, MAX_INPUT_LENGTH + 1, "%lu.%0*lu", integerPart,
             fractionalDigits, fractionalPart);
  } else {
    snprintf(inputBuffer, MAX_INPUT_LENGTH + 1, "%lu", integerPart);
  }

  cursorPos = strlen(inputBuffer);
  dotEntered = strchr(inputBuffer, '.') != NULL;
} */

static void fillFromCurrentValue() {
  uint32_t value =
      (inputStage == INPUT_FIRST_VALUE) ? gFInputValue1 : gFInputValue2;

  // Skip filling if value is 0 to prevent displaying initial "0"
  if (value == 0) {
    resetInputBuffer();
    return;
  }

  uint32_t integerPart = 0;
  uint32_t fractionalPart = 0;
  uint8_t displayFractionalDigits = 0;

  // Конвертируем из нативных единиц в отображаемые
  switch (currentConfig.unit) {
  case UNIT_HZ:
    integerPart = value;
    fractionalPart = 0;
    displayFractionalDigits = 0;
    break;
  case UNIT_KHZ:
    integerPart = value / 100;
    fractionalPart = value % 100;
    displayFractionalDigits = 2; // показываем до 2 знаков после запятой
    break;
  case UNIT_MHZ:
    integerPart = value / 100000;
    fractionalPart = value % 100000;
    displayFractionalDigits = 5; // показываем до 5 знаков после запятой
    break;
  case UNIT_VOLTS:
    integerPart = value / 100;
    fractionalPart = value % 100;
    displayFractionalDigits = 2;
    break;
  default:
    integerPart = value;
    break;
  }

  // Убираем trailing zeros
  if (displayFractionalDigits > 0 && fractionalPart > 0) {
    while (fractionalPart % 10 == 0 && displayFractionalDigits > 0) {
      fractionalPart /= 10;
      displayFractionalDigits--;
    }
    snprintf(inputBuffer, MAX_INPUT_LENGTH + 1, "%lu.%0*lu", integerPart,
             displayFractionalDigits, fractionalPart);
  } else {
    snprintf(inputBuffer, MAX_INPUT_LENGTH + 1, "%lu", integerPart);
  }

  cursorPos = strlen(inputBuffer);
  dotEntered = strchr(inputBuffer, '.') != NULL;
  fractionalDigits = displayFractionalDigits;
}

/* void FINPUT_setup(uint32_t min, uint32_t max, InputUnit unit, bool is_range) {
  currentConfig.min = min;
  currentConfig.max = max;
  currentConfig.unit = unit;
  currentConfig.is_range = is_range;
  currentConfig.allow_dot =
      (unit == UNIT_MHZ || unit == UNIT_KHZ || unit == UNIT_VOLTS);

  switch (unit) {
  case UNIT_HZ:
    currentConfig.multiplier = HZ;
    break;
  case UNIT_KHZ:
    currentConfig.multiplier = KHZ;
    break;
  case UNIT_MHZ:
    currentConfig.multiplier = MHZ;
    break;
  case UNIT_VOLTS:
    currentConfig.multiplier = 100;
    break;
  default:
    currentConfig.multiplier = 1;
    break;
  }
} */

/* void FINPUT_init() {
  uint32_t maxDisplayValue;
  switch (currentConfig.unit) {
  case UNIT_MHZ:
    maxDisplayValue = currentConfig.max / MHZ;
    break;
  case UNIT_KHZ:
    maxDisplayValue = currentConfig.max / KHZ;
    break;
  case UNIT_HZ:
    maxDisplayValue = currentConfig.max / HZ;
    break;
  case UNIT_VOLTS:
    maxDisplayValue = currentConfig.max / 100;
    break;
  default:
    maxDisplayValue = currentConfig.max;
    break;
  }

  currentConfig.max_digits = 0;
  while (maxDisplayValue > 0) {
    maxDisplayValue /= 10;
    currentConfig.max_digits++;
  }

  if (currentConfig.max_digits > MAX_INPUT_LENGTH - 1) {
    currentConfig.max_digits = MAX_INPUT_LENGTH - 1;
  }

  reset();
  if (currentConfig.is_range && gFInputValue1 != 0) {
    inputStage = INPUT_SECOND_VALUE;
    fillFromCurrentValue();
  } else {
    fillFromCurrentValue();
  }
} */

void FINPUT_setup(uint32_t min, uint32_t max, InputUnit unit, bool is_range) {
  // min и max уже в нативных единицах (десятках Герц)
  // unit используется только для отображения
  currentConfig.min = min;
  currentConfig.max = max;
  currentConfig.unit = unit;
  currentConfig.is_range = is_range;
  currentConfig.allow_dot =
      (unit == UNIT_MHZ || unit == UNIT_KHZ || unit == UNIT_VOLTS);

  // multiplier = 1, т.к. работаем напрямую с десятками Герц
  switch (unit) {
  case UNIT_HZ:
    currentConfig.multiplier = 1; // 1 десяток Герц = 1 единица
    break;
  case UNIT_KHZ:
    currentConfig.multiplier = 100; // 1 кГц = 100 десятков Герц
    break;
  case UNIT_MHZ:
    currentConfig.multiplier = 100000; // 1 МГц = 100000 десятков Герц
    break;
  case UNIT_VOLTS:
    currentConfig.multiplier = 100;
    break;
  default:
    currentConfig.multiplier = 1;
    break;
  }
}

void FINPUT_init() {
  uint32_t maxDisplayValue;
  // Конвертируем из нативных единиц в отображаемые
  switch (currentConfig.unit) {
  case UNIT_MHZ:
    maxDisplayValue = currentConfig.max / 100000;
    break;
  case UNIT_KHZ:
    maxDisplayValue = currentConfig.max / 100;
    break;
  case UNIT_HZ:
    maxDisplayValue = currentConfig.max;
    break;
  case UNIT_VOLTS:
    maxDisplayValue = currentConfig.max / 100;
    break;
  default:
    maxDisplayValue = currentConfig.max;
    break;
  }

  currentConfig.max_digits = 0;
  while (maxDisplayValue > 0) {
    maxDisplayValue /= 10;
    currentConfig.max_digits++;
  }

  if (currentConfig.max_digits > MAX_INPUT_LENGTH - 1) {
    currentConfig.max_digits = MAX_INPUT_LENGTH - 1;
  }

  reset();
  if (currentConfig.is_range && gFInputValue1 != 0) {
    inputStage = INPUT_SECOND_VALUE;
    fillFromCurrentValue();
  } else {
    fillFromCurrentValue();
  }
}

void FINPUT_update() {
  if (Now() - lastUpdate >= 500) {
    blinkState = !blinkState;
    gRedrawScreen = true;
    lastUpdate = Now();
  }
}

void FINPUT_deinit(void) {}

bool FINPUT_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_LONG_PRESSED && key == KEY_EXIT) {
    reset();
    gRedrawScreen = true;
    return true;
  }

  if (state == KEY_RELEASED) {
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
      if (cursorPos < MAX_INPUT_LENGTH - 1) {
        if (dotEntered && fractionalDigits >= MAX_FRACTION_DIGITS) {
          break;
        }

        // Check if adding new digit would exceed max value
        uint32_t currentValue = convertFromDisplayValue();
        uint32_t newDigit = key - KEY_0;
        uint32_t newValue =
            currentValue * 10 + newDigit * currentConfig.multiplier;

        if (!dotEntered && currentConfig.allow_dot &&
            newValue > currentConfig.max && cursorPos > 0) {
          // Insert decimal point if value would exceed max
          inputBuffer[cursorPos++] = '.';
          inputBuffer[cursorPos] = '\0';
          dotEntered = true;
          gRedrawScreen = true;
        }

        if (cursorPos < MAX_INPUT_LENGTH - 1) {
          inputBuffer[cursorPos++] = '0' + (key - KEY_0);
          inputBuffer[cursorPos] = '\0';
          if (dotEntered)
            fractionalDigits++;

          if (inputStage == INPUT_FIRST_VALUE) {
            gFInputValue1 = convertFromDisplayValue();
          } else {
            gFInputValue2 = convertFromDisplayValue();
          }
          gRedrawScreen = true;
        }
      }
      return true;

    case KEY_STAR:
      if (currentConfig.allow_dot && !dotEntered && cursorPos > 0 &&
          cursorPos < MAX_INPUT_LENGTH - 1) {
        inputBuffer[cursorPos++] = '.';
        inputBuffer[cursorPos] = '\0';
        dotEntered = true;
        gRedrawScreen = true;
        return true;
      }
      if (currentConfig.is_range && cursorPos > 0 &&
          inputStage == INPUT_FIRST_VALUE) {
        gFInputValue1 = convertFromDisplayValue();
        inputStage = INPUT_SECOND_VALUE;
        resetInputBuffer();
        gRedrawScreen = true;
        return true;
      }
      break;

    case KEY_EXIT:
      if (cursorPos == 0) {
        if (currentConfig.is_range && inputStage == INPUT_SECOND_VALUE) {
          inputStage = INPUT_FIRST_VALUE;
          resetInputBuffer();
          fillFromCurrentValue();
          gRedrawScreen = true;
          return true;
        }
        gFInputCallback = NULL;
        APPS_exit();
        return true;
      }

      if (cursorPos > 0) {
        if (inputBuffer[cursorPos - 1] == '.') {
          dotEntered = false;
        } else if (dotEntered && fractionalDigits > 0) {
          fractionalDigits--;
        }
        inputBuffer[--cursorPos] = '\0';

        if (inputStage == INPUT_FIRST_VALUE) {
          gFInputValue1 = convertFromDisplayValue();
        } else {
          gFInputValue2 = convertFromDisplayValue();
        }
        gRedrawScreen = true;
      }
      return true;

    case KEY_MENU:
    case KEY_F:
    case KEY_PTT:
      if (inputStage == INPUT_FIRST_VALUE) {
        gFInputValue1 = convertFromDisplayValue();

        if (currentConfig.is_range) {
          inputStage = INPUT_SECOND_VALUE;
          resetInputBuffer();
          gRedrawScreen = true;
          return true;
        }
      }

      if (gFInputValue1 >= currentConfig.min &&
          gFInputValue1 <= currentConfig.max) {
        if (currentConfig.is_range && inputStage == INPUT_SECOND_VALUE) {
          gFInputValue2 = convertFromDisplayValue();
          if (gFInputValue2 < gFInputValue1) {
            // Swap values if second is less than first
            uint32_t temp = gFInputValue1;
            gFInputValue1 = gFInputValue2;
            gFInputValue2 = temp;
          }
        }

        APPS_exit();
        if (gFInputCallback) {
          gFInputCallback(gFInputValue1,
                          currentConfig.is_range ? gFInputValue2 : 0);
        }
        gFInputCallback = NULL;
        gFInputValue1 = 0;
        gFInputValue2 = 0;
      }
      return true;

    default:
      break;
    }
  }
  return false;
}

void FINPUT_render(void) {
  const uint8_t BASE_Y = 32;
  char displayStr[MAX_INPUT_LENGTH + 3] = "";

  // Only copy buffer to display if it's not empty
  if (inputBuffer[0] != '\0') {
    strncpy(displayStr, inputBuffer, MAX_INPUT_LENGTH);
  }

  const char *unitSuffix = "";
  switch (currentConfig.unit) {
  case UNIT_HZ:
    unitSuffix = "Hz";
    break;
  case UNIT_KHZ:
    unitSuffix = "kHz";
    break;
  case UNIT_MHZ:
    unitSuffix = "MHz";
    break;
  case UNIT_VOLTS:
    unitSuffix = "V";
    break;
  case UNIT_DBM:
    unitSuffix = "dBm";
    break;
  case UNIT_PERCENT:
    unitSuffix = "%";
    break;
  default:
    unitSuffix = "";
    break;
  }

  // Отображаем текущий ввод
  PrintBigDigitsEx(LCD_WIDTH - 3, BASE_Y, POS_R, C_FILL, "%s", displayStr);
  PrintMediumEx(LCD_WIDTH - 3, BASE_Y + 8, POS_R, C_FILL, "%s", unitSuffix);

  // Отображаем курсор
  if (blinkState && cursorPos < currentConfig.max_digits +
                                    (dotEntered ? MAX_FRACTION_DIGITS : 0)) {
    uint8_t cursorX =
        LCD_WIDTH - 1 - (strlen(displayStr) - cursorPos) * BIG_DIGIT_WIDTH;
    FillRect(cursorX, BASE_Y - BIG_DIGIT_HEIGHT + 1, 1, BIG_DIGIT_HEIGHT,
             C_FILL);
  }

  // Отображаем информацию о диапазоне
  if (currentConfig.is_range) {
    char rangeStr[32];
    if (inputStage == INPUT_FIRST_VALUE) {
      if (cursorPos > 0) {
        snprintf(rangeStr, sizeof(rangeStr), "Start: %lu",
                 convertFromDisplayValue());
      } else {
        snprintf(rangeStr, sizeof(rangeStr), "Start: %lu", gFInputValue1);
      }
    } else {
      snprintf(rangeStr, sizeof(rangeStr), "End: %lu-%lu", gFInputValue1,
               cursorPos > 0 ? convertFromDisplayValue() : gFInputValue1);
    }
    PrintSmall(0, 16, rangeStr);
  }

  /* if (currentConfig.min) {
    switch (currentConfig.unit) {
    case UNIT_MHZ:
      PrintSmall(0, 32, "Min: %u.%05u MHz", currentConfig.min / MHZ,
                 currentConfig.min % MHZ);
      break;
    case UNIT_KHZ:
      PrintSmall(0, 32, "Min: %u.%05u kHz", currentConfig.min / KHZ,
                 currentConfig.min % KHZ);
      break;
    case UNIT_HZ:
      PrintSmall(0, 32, "Min: %u", currentConfig.min * 10);
      break;
    case UNIT_VOLTS:
      PrintSmall(0, 32, "Min: %u.%02u", currentConfig.min / 100,
                 currentConfig.min % 100);
      break;
    default:
      PrintSmall(0, 32, "Min: %u", currentConfig.min);
      break;
    }
  }

  if (currentConfig.max) {
    switch (currentConfig.unit) {
    case UNIT_MHZ:
      PrintSmall(0, 42, "max: %u.%05u MHz", currentConfig.max / MHZ,
                 currentConfig.max % MHZ);
      break;
    case UNIT_KHZ:
      PrintSmall(0, 42, "max: %u.%05u kHz", currentConfig.max / KHZ,
                 currentConfig.max % KHZ);
      break;
    case UNIT_HZ:
      PrintSmall(0, 42, "max: %u", currentConfig.max * 10);
      break;
    case UNIT_VOLTS:
      PrintSmall(0, 42, "max: %u.%02u", currentConfig.max / 100,
                 currentConfig.max % 100);
      break;
    default:
      PrintSmall(0, 42, "max: %u", currentConfig.max);
      break;
    }
  } */

    if (currentConfig.min) {
    switch (currentConfig.unit) {
    case UNIT_MHZ:
      PrintSmall(0, 32, "Min: %u.%05u MHz", currentConfig.min / 100000,
                 currentConfig.min % 100000);
      break;
    case UNIT_KHZ:
      PrintSmall(0, 32, "Min: %u.%02u kHz", currentConfig.min / 100,
                 currentConfig.min % 100);
      break;
    case UNIT_HZ:
      PrintSmall(0, 32, "Min: %u Hz", currentConfig.min);
      break;
    case UNIT_VOLTS:
      PrintSmall(0, 32, "Min: %u.%02u V", currentConfig.min / 100,
                 currentConfig.min % 100);
      break;
    default:
      PrintSmall(0, 32, "Min: %u", currentConfig.min);
      break;
    }
  }

  if (currentConfig.max) {
    switch (currentConfig.unit) {
    case UNIT_MHZ:
      PrintSmall(0, 42, "Max: %u.%05u MHz", currentConfig.max / 100000,
                 currentConfig.max % 100000);
      break;
    case UNIT_KHZ:
      PrintSmall(0, 42, "Max: %u.%02u kHz", currentConfig.max / 100,
                 currentConfig.max % 100);
      break;
    case UNIT_HZ:
      PrintSmall(0, 42, "Max: %u Hz", currentConfig.max);
      break;
    case UNIT_VOLTS:
      PrintSmall(0, 42, "Max: %u.%02u V", currentConfig.max / 100,
                 currentConfig.max % 100);
      break;
    default:
      PrintSmall(0, 42, "Max: %u", currentConfig.max);
      break;
    }
  }
}
