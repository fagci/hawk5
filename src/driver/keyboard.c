#include "keyboard.h"
#include "../inc/dp32g030/gpio.h"
#include "../scheduler.h"
#include "../system.h"
#include "gpio.h"
#include "systick.h"

static SystemMessages n;

#define LONG_PRESS_TIME 500
#define LONG_PRESS_REPEAT_TIME 100
#define KEYBOARD_ROWS 5
#define KEYBOARD_COLS 4

static bool mKeyPtt;
static KEY_Code_t mKeyPressed = KEY_INVALID;
static KEY_Code_t mPrevKeyPressed = KEY_INVALID;
static Key_State_t mPrevStatePtt;
static Key_State_t mPrevKeyState;
static uint32_t mLongPressTimer;
static uint32_t mLongPressRepeatTimer;

static uint32_t mPttLongPressTimer;
static uint32_t mPttLongPressRepeatTimer;

typedef const struct {
  uint16_t setToZeroMask;
  struct {
    uint8_t key : 5;
    uint8_t pin : 3;
  } pins[4];
} Keyboard;

static Keyboard keyboard[5] = {
    /* Zero row  */
    {.setToZeroMask = 0xffff,
     .pins =
         {
             {.key = KEY_SIDE1, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_SIDE2, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_SIDE2, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_SIDE2, .pin = GPIOA_PIN_KEYBOARD_1},
         }},
    /* First row  */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_4) & 0xffff,
     .pins =
         {
             {.key = KEY_MENU, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_1, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_4, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_7, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
    /* Second row */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_5) & 0xffff,
     .pins =
         {
             {.key = KEY_UP, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_2, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_5, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_8, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
    /* Third row */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_6) & 0xffff,
     .pins =
         {
             {.key = KEY_DOWN, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_3, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_6, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_9, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
    /* Fourth row */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_7) & 0xffff,
     .pins =
         {
             {.key = KEY_EXIT, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_STAR, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_0, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_F, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
};

static void HandlePttKey(void) {
  uint32_t currentTick = Now();
  mKeyPtt = !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT);

  if (mKeyPtt) {
    if (mPrevStatePtt == KEY_RELEASED) {
      SYS_MsgKey(KEY_PTT, KEY_PRESSED);
      mPrevStatePtt = KEY_PRESSED;
      mPttLongPressTimer = currentTick;
    } else if (mPrevStatePtt == KEY_PRESSED) {
      uint32_t elapsedTime = currentTick - mPttLongPressTimer;
      if (elapsedTime >= LONG_PRESS_TIME) {
        SYS_MsgKey(KEY_PTT, KEY_LONG_PRESSED);
        mPttLongPressTimer = 0;
        mPrevStatePtt = KEY_LONG_PRESSED;
        mPttLongPressRepeatTimer = currentTick;
      }
    } else if (mPrevStatePtt == KEY_LONG_PRESSED ||
               mPrevStatePtt == KEY_LONG_PRESSED_CONT) {
      uint32_t elapsedTime = currentTick - mPttLongPressRepeatTimer;
      if (elapsedTime >= LONG_PRESS_REPEAT_TIME) {
        mPttLongPressRepeatTimer = currentTick;
        SYS_MsgKey(KEY_PTT, KEY_LONG_PRESSED_CONT);
        mPrevStatePtt = KEY_LONG_PRESSED_CONT;
      }
    }
  } else {
    if (mPrevStatePtt != KEY_RELEASED) {
      if (mPrevStatePtt == KEY_PRESSED) {
        SYS_MsgKey(KEY_PTT, KEY_RELEASED);
      }
      if (mPrevStatePtt == KEY_LONG_PRESSED ||
          mPrevStatePtt == KEY_LONG_PRESSED_CONT) {
        SYS_MsgKey(KEY_PTT, KEY_RELEASED);
      }

      mPttLongPressTimer = 0;
      mPrevStatePtt = KEY_RELEASED;
    }
  }
}

static void ResetKeyboardRow(uint8_t row) {
  GPIOA->DATA |= (1u << GPIOA_PIN_KEYBOARD_4) | (1u << GPIOA_PIN_KEYBOARD_5) |
                 (1u << GPIOA_PIN_KEYBOARD_6) | (1u << GPIOA_PIN_KEYBOARD_7);
  GPIOA->DATA &= keyboard[row].setToZeroMask;
}

static uint16_t ReadStableGpioData(void) {
  uint16_t reg, reg2;
  uint8_t ii;

  for (ii = 0, reg = 0; ii < 3; ii++) {
    TIMER_DelayUs(1);
    reg2 = (uint16_t)GPIOA->DATA;
    if (reg != reg2) {
      reg = reg2;
      ii = 0;
    }
  }
  return reg;
}

static void ResetKeyboardPins(void) {
  GPIOA->DATA = (GPIOA->DATA & ~(1u << GPIOA_PIN_KEYBOARD_6)) |
                (1u << GPIOA_PIN_KEYBOARD_7);
}

static uint8_t ScanKeyboardMatrix(void) {
  for (uint8_t i = 0; i < KEYBOARD_ROWS; i++) {
    ResetKeyboardRow(i);
    uint16_t reg = ReadStableGpioData();

    for (uint8_t j = 0; j < KEYBOARD_COLS; j++) {
      const uint16_t mask = 1u << keyboard[i].pins[j].pin;
      if (!(reg & mask)) {
        return keyboard[i].pins[j].key;
      }
    }
  }
  return KEY_INVALID;
}

void KEYBOARD_Poll(void) {
  HandlePttKey();
  mKeyPressed = ScanKeyboardMatrix();
  ResetKeyboardPins();
}

void SYS_MsgKey(KEY_Code_t key, Key_State_t state) {
  n.message = MSG_KEYPRESSED;
  n.key = key;
  n.state = state;
}

void KEYBOARD_CheckKeys(void) {
  uint32_t currentTick = Now();

  if (mKeyPressed != KEY_INVALID) {
    if (mPrevKeyState == KEY_RELEASED) {
      SYS_MsgKey(mKeyPressed, KEY_PRESSED);
      mPrevKeyState = KEY_PRESSED;

      mLongPressTimer = currentTick;
      mPrevKeyPressed = mKeyPressed;
    } else if (mPrevKeyState == KEY_PRESSED) {
      uint32_t elapsedTime = currentTick - mLongPressTimer;
      if (elapsedTime >= LONG_PRESS_TIME) {
        SYS_MsgKey(mKeyPressed, KEY_LONG_PRESSED);
        mLongPressTimer = 0;
        mPrevKeyState = KEY_LONG_PRESSED;
        mLongPressRepeatTimer = currentTick;
      }
    } else if (mPrevKeyState == KEY_LONG_PRESSED ||
               mPrevKeyState == KEY_LONG_PRESSED_CONT) {
      uint32_t elapsedTime = currentTick - mLongPressRepeatTimer;
      if (elapsedTime >= LONG_PRESS_REPEAT_TIME) {
        mLongPressRepeatTimer = currentTick;
        SYS_MsgKey(mKeyPressed, KEY_LONG_PRESSED_CONT);
        mPrevKeyState = KEY_LONG_PRESSED_CONT;
      }
    }
  } else {
    if (mPrevKeyState != KEY_RELEASED) {
      if (mPrevKeyState != KEY_LONG_PRESSED &&
          mPrevKeyState != KEY_LONG_PRESSED_CONT) {
        SYS_MsgKey(mPrevKeyPressed, KEY_RELEASED);
      }

      if (mPrevKeyState == KEY_LONG_PRESSED_CONT &&
          (mPrevKeyPressed == KEY_UP || mPrevKeyPressed == KEY_DOWN)) {
        SYS_MsgKey(mPrevKeyPressed, KEY_RELEASED);
      }

      mLongPressTimer = 0;
      mPrevKeyState = KEY_RELEASED;
      mPrevKeyPressed = KEY_INVALID;
    }
  }
}

SystemMessages KEYBOARD_GetKey(void) {
  n.message = MSG_NONE;
  KEYBOARD_Poll();
  KEYBOARD_CheckKeys();
  return n;
}
