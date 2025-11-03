#include "eeprom.h"
#include "../external/printf/printf.h"
#include "../settings.h"
#include "i2c.h"
#include "system.h"
#include "systick.h"
#include "uart.h"
#include <stddef.h>
#include <string.h>

bool gEepromWrite = false;

static uint8_t tmpBuffer[128];

/* Глобальные параметры EEPROM (определяются автоматически) */
static uint32_t g_eeprom_size = 0;
static uint16_t g_eeprom_page_size = 0;

/* Инициализация и определение параметров EEPROM */
void EEPROM_Init(void) {
  // printf("\n=== EEPROM Initialization ===\n");

  /* Определяем размер */
  // g_eeprom_size = EEPROM_DetectSize();
  g_eeprom_size = SETTINGS_GetEEPROMSize();

  /* Определяем page size */
  g_eeprom_page_size = EEPROM_GetPageSize();
  // printf("Page size: %u bytes\n", g_eeprom_page_size);

  // printf("=== EEPROM Ready ===\n\n");
}

/* Ожидание готовности EEPROM после записи (polling) */
bool EEPROM_WaitReady(uint8_t device_addr, uint32_t timeout_ms) {
  uint64_t start = GetUptimeUs();
  uint32_t attempts = 0;

  while ((GetUptimeUs() - start) < (timeout_ms * 1000ULL)) {
    I2C_Start();
    int result = I2C_Write(device_addr);
    I2C_Stop();

    attempts++;

    if (result == 0) {
      /* if (attempts > 1) {
        printf("EEPROM: Ready after %lu attempts\n", attempts);
      } */
      return true;
    }

    TIMER_DelayUs(100); // Увеличил паузу между попытками
  }

  // printf("EEPROM: Timeout after %lu attempts\n", attempts);
  return false;
}

/* Чтение из EEPROM */
int EEPROM_ReadBuffer(uint32_t address, void *pBuffer, uint16_t size) {
  if (size == 0)
    return 0;

  /* M24M02: маска адреса для 256KB */
  const uint32_t EEPROM_SIZE_MASK = 0x3FFFF;
  address &= EEPROM_SIZE_MASK;

  /* M24M02: биты [17:16] адреса идут в device address
   * 0xA0 | (A17<<2) | (A16<<1) */
  uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

  /* Ждём готовности перед чтением */
  /* if (!EEPROM_WaitReady(IIC_ADD, 1)) { // Увеличил до 100ms
    // printf("EEPROM_Read: Not ready\n");
    return -1;
  } */

  /* Небольшая пауза после polling */
  // TIMER_DelayUs(100);

  /* Фаза 1: Записываем адрес (Write mode) */
  I2C_Start();

  if (I2C_Write(IIC_ADD) != 0) {
    // printf("EEPROM_Read: Device addr NACK (0x%02X)\n", IIC_ADD);
    I2C_Stop();
    return -1;
  }

  if (I2C_Write((address >> 8) & 0xFF) != 0) {
    // printf("EEPROM_Read: Addr high NACK\n");
    I2C_Stop();
    return -1;
  }

  if (I2C_Write(address & 0xFF) != 0) {
    // printf("EEPROM_Read: Addr low NACK\n");
    I2C_Stop();
    return -1;
  }

  /* Фаза 2: Repeated START + чтение (Read mode) */
  I2C_RepStart();

  if (I2C_Write(IIC_ADD | 0x01) != 0) {
    // printf("EEPROM_Read: Read mode NACK\n");
    I2C_Stop();
    return -1;
  }

  /* Читаем данные */
  I2C_ReadBuffer(pBuffer, size);
  I2C_Stop();

  return 0;
}

/* Запись в EEPROM с учётом границ страниц */
void EEPROM_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint16_t size) {
  if (pBuffer == NULL || size == 0) {
    return;
  }

  /* Используем умное определение page size */
  uint16_t PAGE_SIZE = EEPROM_GetPageSize();

  /* M24M02: 256KB (0x00000 - 0x3FFFF)
   * Адресация: биты [17:16] идут в device address, [15:0] в память */
  const uint32_t EEPROM_SIZE_MASK = 0x3FFFF; // 256KB для M24M02
  uint32_t masked_addr = address & EEPROM_SIZE_MASK;

  /* if (address != masked_addr) {
    printf("EEPROM_Write: WARNING! Address 0x%05lX wrapped to 0x%05lX\n",
           address, masked_addr);
  } */

  address = masked_addr;

  /* printf("EEPROM_Write: addr=0x%05lX, size=%u, page_size=%u\n", address,
     size, PAGE_SIZE); */

  while (size > 0) {
    /* Вычисляем сколько байт можно записать до конца страницы */
    uint16_t page_offset = address % PAGE_SIZE;
    uint16_t bytes_to_end = PAGE_SIZE - page_offset;
    uint16_t chunk_size = (size < bytes_to_end) ? size : bytes_to_end;

    uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

    /* printf("  Chunk: addr=0x%05lX, size=%u, dev=0x%02X\n", address,
       chunk_size, IIC_ADD); */

    /* Ждём готовности перед записью */
    if (!EEPROM_WaitReady(IIC_ADD, 1)) {
      // printf("EEPROM_Write: Not ready at 0x%04lX\n", address);
      return;
    }

    /* Пауза после polling */
    TIMER_DelayUs(100);

    /* Записываем chunk */
    I2C_Start();

    if (I2C_Write(IIC_ADD) != 0) {
      // printf("EEPROM_Write: Device addr NACK (0x%02X)\n", IIC_ADD);
      I2C_Stop();
      return;
    }

    if (I2C_Write((address >> 8) & 0xFF) != 0) {
      // printf("EEPROM_Write: Addr high NACK\n");
      I2C_Stop();
      return;
    }

    if (I2C_Write(address & 0xFF) != 0) {
      // printf("EEPROM_Write: Addr low NACK\n");
      I2C_Stop();
      return;
    }

    /* ВАЖНО: проверяем каждый байт данных */
    for (uint16_t i = 0; i < chunk_size; i++) {
      int result = I2C_Write(pBuffer[i]);
      if (result != 0) {
        /* printf("EEPROM_Write: Data NACK at byte %u (value=0x%02X)\n", i,
               pBuffer[i]); */
        I2C_Stop();
        return;
      }
    }

    I2C_Stop();

    /* КРИТИЧНО: Ждём завершения internal write cycle */
    // printf("  Written %u bytes, waiting write cycle...\n", chunk_size);
    SYS_DelayMs(10);

    /* ВАЖНО: Polling для гарантии готовности */
    if (!EEPROM_WaitReady(IIC_ADD, 50)) {
      // printf("EEPROM_Write: Device not ready after write\n");
      return;
    }

    /* Переходим к следующему chunk */
    pBuffer += chunk_size;
    address += chunk_size;
    size -= chunk_size;

    gEepromWrite = true;
  }

  // printf("EEPROM_Write: Complete\n");
}

/* Очистка страницы (заполнение 0xFF) */
void EEPROM_ClearPage(uint16_t page) {
  uint16_t PAGE_SIZE = EEPROM_GetPageSize();
  const uint32_t address = page * PAGE_SIZE;

  uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

  /* Ждём готовности */
  if (!EEPROM_WaitReady(IIC_ADD, 100)) {
    // printf("EEPROM_Clear: Not ready\n");
    return;
  }

  TIMER_DelayUs(100);

  I2C_Start();

  if (I2C_Write(IIC_ADD) != 0) {
    // printf("EEPROM_Clear: Device addr NACK\n");
    I2C_Stop();
    return;
  }

  if (I2C_Write((address >> 8) & 0xFF) != 0) {
    // printf("EEPROM_Clear: Addr high NACK\n");
    I2C_Stop();
    return;
  }

  if (I2C_Write(address & 0xFF) != 0) {
    // printf("EEPROM_Clear: Addr low NACK\n");
    I2C_Stop();
    return;
  }

  /* Заполняем страницу */
  for (uint16_t i = 0; i < PAGE_SIZE; ++i) {
    if (I2C_Write(0xFF) != 0) {
      // printf("EEPROM_Clear: Write failed at offset %u\n", i);
      I2C_Stop();
      return;
    }
  }

  I2C_Stop();
  SYS_DelayMs(10);

  gEepromWrite = true;
}

/* === Диагностические функции === */

/* Получение правильного page size для EEPROM */
uint16_t EEPROM_GetPageSize(void) {
  /* Если уже определили - возвращаем кешированное значение */
  if (g_eeprom_page_size != 0) {
    return g_eeprom_page_size;
  }

  /* Пытаемся получить из настроек */
  uint16_t page_size = SETTINGS_GetPageSize();
  return page_size;

  /* Валидация: page_size должен быть степенью 2 и в диапазоне 8-256 */
  if (page_size >= 8 && page_size <= 256 &&
      (page_size & (page_size - 1)) == 0) { // Проверка степени 2
    return page_size;
  }

  /* Определяем по типу EEPROM */
  uint8_t found_devices = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (EEPROM_Detect(0xA0 | (i << 1))) {
      found_devices++;
    }
  }

  /* M24M02 (4 devices, 256KB): page = 256 bytes
   * 24C256 (4 devices, 32KB):  page = 64 bytes
   * 24C128 (2 devices, 16KB):  page = 64 bytes
   * 24C64  (1 device, 8KB):    page = 32 bytes */

  if (found_devices == 4) {
    return 256; // M24M02 most likely
  } else if (found_devices == 2) {
    return 64; // 24C128
  } else {
    return 32; // 24C64 or smaller
  }
}

/* Определение размера EEPROM */
uint32_t EEPROM_DetectSize(void) {
  // printf("\nDetecting EEPROM size...\n");

  /* Проверяем какие device addresses отвечают */
  uint8_t found_devices = 0;
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t addr = 0xA0 | (i << 1);
    if (EEPROM_Detect(addr)) {
      // printf("  Device at 0x%02X\n", addr);
      found_devices++;
    }
  }

  uint32_t size = 0;

  /* Определяем размер по количеству устройств:
   * 24C32/64:  1 device  (0xA0)         → 4-8 KB
   * 24C128:    2 devices (0xA0, 0xA2)   → 16 KB
   * 24C256:    4 devices (0xA0-0xA6)    → 32 KB
   * M24M02:    4 devices (0xA0-0xA6)    → 256 KB (!!)
   * 24C512:    8 devices (0xA0-0xAE)    → 64 KB */

  switch (found_devices) {
  case 1:
    size = 8192;
    break; // 8 KB (24C64)
  case 2:
    size = 16384;
    break; // 16 KB (24C128)
  case 4:
    size = 262144;
    break; // 256 KB (M24M02) - most likely!
  case 8:
    size = 65536;
    break; // 64 KB (24C512)
  default:
    size = 262144;
    break; // Default to M24M02
  }

  /* printf("  Detected size: %lu bytes (%lu KB)\n", size, size / 1024);
  printf("  Address mask: 0x%05lX\n", size - 1);
  printf("  Device type: %s\n", (size == 262144)  ? "M24M02 (256KB)"
                                : (size == 65536) ? "24C512 (64KB)"
                                : (size == 32768) ? "24C256 (32KB)"
                                : (size == 16384) ? "24C128 (16KB)"
                                                  : "24C64 (8KB)"); */

  return size;
}

/* Сканирование I2C шины */
void EEPROM_ScanBus(void) {
  printf("\nI2C Bus Scan:\n");
  printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

  for (uint8_t row = 0; row < 8; row++) {
    printf("%02X: ", row * 16);

    for (uint8_t col = 0; col < 16; col++) {
      uint8_t addr = (row * 16 + col) << 1; // 7-bit addr -> 8-bit

      if (addr < 0x10 || addr > 0xF0) {
        printf("   "); // Зарезервированные адреса
        continue;
      }

      I2C_Start();
      int result = I2C_Write(addr);
      I2C_Stop();

      if (result == 0) {
        printf("%02X ", addr);
      } else {
        printf("-- ");
      }

      TIMER_DelayUs(100);
    }
    printf("\n");
  }
  printf("\n");
}

/* Чтение одного байта */
int EEPROM_ReadByte(uint32_t address, uint8_t *value) {
  return EEPROM_ReadBuffer(address, value, 1);
}

/* Запись одного байта */
int EEPROM_WriteByte(uint32_t address, uint8_t value) {
  EEPROM_WriteBuffer(address, &value, 1);
  return 0;
}

/* Проверка наличия EEPROM */
bool EEPROM_Detect(uint8_t device_addr) {
  I2C_Start();
  int result = I2C_Write(device_addr);
  I2C_Stop();

  return (result == 0);
}

/* Улучшенный тест с подробной диагностикой */
bool EEPROM_Test(uint32_t test_addr) {
  printf("\n=== EEPROM Test at 0x%04lX ===\n", test_addr);

  /* Шаг 1: Проверка адресации */
  uint8_t device_addr = 0xA0 | ((test_addr >> 15) & 0x0E);
  printf("1. Testing device address 0x%02X...", device_addr);

  if (!EEPROM_Detect(device_addr)) {
    printf(" FAIL (no ACK)\n");
    return false;
  }
  printf(" OK\n");

  /* Шаг 2: Попытка записи одного байта */
  printf("2. Writing single byte 0x42...");
  uint8_t test_byte = 0x42;

  I2C_Start();
  if (I2C_Write(device_addr) != 0) {
    printf(" FAIL (device NACK)\n");
    I2C_Stop();
    return false;
  }

  if (I2C_Write((test_addr >> 8) & 0xFF) != 0) {
    printf(" FAIL (addr high NACK)\n");
    I2C_Stop();
    return false;
  }

  if (I2C_Write(test_addr & 0xFF) != 0) {
    printf(" FAIL (addr low NACK)\n");
    I2C_Stop();
    return false;
  }

  if (I2C_Write(test_byte) != 0) {
    printf(" FAIL (data NACK) - CHECK WP PIN!\n");
    I2C_Stop();
    return false;
  }

  I2C_Stop();
  printf(" OK\n");

  /* Шаг 3: Ждём write cycle */
  printf("3. Waiting for write cycle (10ms)...");
  SYS_DelayMs(10);
  printf(" OK\n");

  /* Шаг 4: Чтение обратно */
  printf("4. Reading back...");
  uint8_t read_byte = 0;

  if (EEPROM_ReadBuffer(test_addr, &read_byte, 1) != 0) {
    printf(" FAIL (read error)\n");
    return false;
  }

  if (read_byte == test_byte) {
    printf(" OK (0x%02X)\n", read_byte);
  } else {
    printf(" FAIL (got 0x%02X, expected 0x%02X)\n", read_byte, test_byte);
    return false;
  }

  /* Шаг 5: Полный тест с массивом */
  printf("5. Testing buffer write/read...");
  uint8_t write_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
  uint8_t read_data[8] = {0};

  EEPROM_WriteBuffer(test_addr, write_data, sizeof(write_data));

  if (EEPROM_ReadBuffer(test_addr, read_data, sizeof(read_data)) != 0) {
    printf(" FAIL (read error)\n");
    return false;
  }

  if (memcmp(write_data, read_data, sizeof(write_data)) == 0) {
    printf(" OK\n");
  } else {
    printf(" FAIL (data mismatch)\n");
    printf("  Write: ");
    for (int i = 0; i < 8; i++)
      printf("%02X ", write_data[i]);
    printf("\n  Read:  ");
    for (int i = 0; i < 8; i++)
      printf("%02X ", read_data[i]);
    printf("\n");
    return false;
  }

  printf("\n=== Test PASSED ===\n\n");
  return true;
}
