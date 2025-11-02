#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include "../helper/channels.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  // Reset/Default
  LOG_C_RESET = 0, ///< Reset all attributes

  // Normal colors
  LOG_C_BLACK = 30,   ///< Black
  LOG_C_RED = 31,     ///< Red
  LOG_C_GREEN = 32,   ///< Green
  LOG_C_YELLOW = 33,  ///< Yellow
  LOG_C_BLUE = 34,    ///< Blue
  LOG_C_MAGENTA = 35, ///< Magenta
  LOG_C_CYAN = 36,    ///< Cyan
  LOG_C_WHITE = 37,   ///< White

  // Bright colors
  LOG_C_BRIGHT_BLACK = 90,   ///< Bright black (gray)
  LOG_C_BRIGHT_RED = 91,     ///< Bright red
  LOG_C_BRIGHT_GREEN = 92,   ///< Bright green
  LOG_C_BRIGHT_YELLOW = 93,  ///< Bright yellow
  LOG_C_BRIGHT_BLUE = 94,    ///< Bright blue
  LOG_C_BRIGHT_MAGENTA = 95, ///< Bright magenta
  LOG_C_BRIGHT_CYAN = 96,    ///< Bright cyan
  LOG_C_BRIGHT_WHITE = 97,   ///< Bright white

  // Background colors
  LOG_C_BG_BLACK = 40,   ///< Background black
  LOG_C_BG_RED = 41,     ///< Background red
  LOG_C_BG_GREEN = 42,   ///< Background green
  LOG_C_BG_YELLOW = 43,  ///< Background yellow
  LOG_C_BG_BLUE = 44,    ///< Background blue
  LOG_C_BG_MAGENTA = 45, ///< Background magenta
  LOG_C_BG_CYAN = 46,    ///< Background cyan
  LOG_C_BG_WHITE = 47,   ///< Background white

  // Bright background colors
  LOG_C_BG_BRIGHT_BLACK = 100,   ///< Background bright black
  LOG_C_BG_BRIGHT_RED = 101,     ///< Background bright red
  LOG_C_BG_BRIGHT_GREEN = 102,   ///< Background bright green
  LOG_C_BG_BRIGHT_YELLOW = 103,  ///< Background bright yellow
  LOG_C_BG_BRIGHT_BLUE = 104,    ///< Background bright blue
  LOG_C_BG_BRIGHT_MAGENTA = 105, ///< Background bright magenta
  LOG_C_BG_BRIGHT_CYAN = 106,    ///< Background bright cyan
  LOG_C_BG_BRIGHT_WHITE = 107,   ///< Background bright white

  // Text attributes
  LOG_C_BOLD = 1,         ///< Bold/bright text
  LOG_C_DIM = 2,          ///< Dim text
  LOG_C_ITALIC = 3,       ///< Italic text
  LOG_C_UNDERLINE = 4,    ///< Underlined text
  LOG_C_BLINK = 5,        ///< Blinking text (not widely supported)
  LOG_C_INVERSE = 7,      ///< Inverse colors (swap foreground/background)
  LOG_C_HIDDEN = 8,       ///< Hidden text
  LOG_C_STRIKETHROUGH = 9 ///< Strikethrough text
} LogColor;

void UART_Init(void);
void UART_Send(const void *pBuffer, uint32_t Size);

bool UART_IsCommandAvailable(void);
void UART_HandleCommand(void);
void Log(const char *pattern, ...);
void LogC(LogColor c, const char *pattern, ...);
void LogUart(const char *const str);
void PrintCh(uint16_t chNum, CH *ch);

#endif
