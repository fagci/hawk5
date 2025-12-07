#include "misc.h"
#include "driver/uart.h"
#include "external/printf/printf.h"

char IsPrintable(char ch) { return (ch < 32 || 126 < ch) ? ' ' : ch; }

// return square root of 'value'
unsigned int SQRT16(unsigned int value) {
  unsigned int shift = 16; // number of bits supplied in 'value' .. 2 ~ 32
  unsigned int bit = 1u << --shift;
  unsigned int sqrti = 0;
  while (bit) {
    const unsigned int temp = ((sqrti << 1) | bit) << shift--;
    if (value >= temp) {
      value -= temp;
      sqrti |= bit;
    }
    bit >>= 1;
  }
  return sqrti;
}

void _putchar(char c) { UART_Send((uint8_t *)&c, 1); }

void ScanlistStr(uint32_t sl, char *buf) {
  for (uint8_t i = 0; i < 16; i++) {
    bool sel = sl & (1 << i);
    if (i < 8) {
      buf[i] = sel ? '1' + i : '_';
    } else {
      buf[i] = sel ? 'A' + (i - 8) : '_';
    }
  }
}

void mhzToS(char *buf, uint32_t f) {
  sprintf(buf, "%u.%05u", f / MHZ, f % MHZ);
}

void bkAttToS(char *buf, uint8_t v) {
  sprintf(buf, v == AUTO_GAIN_INDEX ? "Auto" : "%udB", GAIN_TABLE[v].gainDb);
}
