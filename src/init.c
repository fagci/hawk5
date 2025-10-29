/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 * Copyright 2023 Manuel Jedinger
 * https://github.com/manujedi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <stdint.h>
#include <string.h>

// Symbols defined in linker script
extern uint32_t __bss_start__[];
extern uint32_t __bss_end__[];
extern uint32_t _sidata[];      // Source in FLASH
extern uint32_t _sdata[];       // Destination start in RAM
extern uint32_t _edata[];       // Destination end in RAM

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

/**
 * @brief Optimized version using memcpy (if available and smaller)
 * Uncomment this and comment out DATA_Init above if you want to use memcpy
 */
/*
void DATA_Init(void) {
    uint32_t data_size = (uint32_t)_edata - (uint32_t)_sdata;
    if (data_size > 0) {
        memcpy(_sdata, _sidata, data_size);
    }
}
*/

/**
 * @brief Alternative: Combined BSS and DATA init (slightly more compact)
 */
/*
void Memory_Init(void) {
    uint32_t *pBss;
    uint32_t *pSrc = _sidata;
    uint32_t *pDst = _sdata;
    
    // Copy DATA section
    while (pDst < _edata) {
        *pDst++ = *pSrc++;
    }
    
    // Zero BSS section
    for (pBss = __bss_start__; pBss < __bss_end__; pBss++) {
        *pBss = 0;
    }
}
*/
