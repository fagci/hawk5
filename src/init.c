#include <stdint.h>
#include <string.h>

// Symbols defined in linker script
extern uint32_t __bss_start__[];
extern uint32_t __bss_end__[];
extern uint32_t _sidata[]; // Source in FLASH
extern uint32_t _sdata[];  // Destination start in RAM
extern uint32_t _edata[];  // Destination end in RAM

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Initialize BSS section (zero-initialized data)
 */
void BSS_Init(void) {
  uint32_t *pBss;

  for (pBss = __bss_start__; pBss < __bss_end__; pBss++) {
    *pBss = 0;
  }
}

/**
 * @brief Initialize DATA section (copy from FLASH to RAM)
 * This is REQUIRED for initialized global variables to work correctly!
 */
void DATA_Init(void) {
  uint32_t *pSrc = _sidata;
  uint32_t *pDst = _sdata;

  // Copy word-by-word for better performance
  while (pDst < _edata) {
    *pDst++ = *pSrc++;
  }
}

void _init(void) {
    // Пустая функция, но линкер доволен
}

#ifdef __cplusplus
}
#endif
