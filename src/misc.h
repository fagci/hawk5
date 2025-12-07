#ifndef MISC_H
#define MISC_H

#include <stdint.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#define MAKE_WORD(hb, lb) (((uint8_t)(hb) << 8U) | (uint8_t)lb)
#define SWAP(a, b)                                                             \
  {                                                                            \
    __typeof__(a) t = (a);                                                     \
    a = b;                                                                     \
    b = t;                                                                     \
  }

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

char IsPrintable(char ch);
unsigned int SQRT16(unsigned int value);

#define KHZ 100
#define MHZ 100000

void _putchar(char c);
void ScanlistStr(uint32_t sl, char *buf);
void mhzToS(char *buf, uint32_t f);
void bkAttToS(char *buf, uint8_t v);

#endif /* end of include guard: MISC_H */
