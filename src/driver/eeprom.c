#include "eeprom.h"
#include "../external/printf/printf.h"
#include "i2c.h"
#include "system.h"
#include "systick.h"
#include "uart.h"
#include <string.h>

bool gEepromWrite = false;

static uint32_t g_eeprom_size = 0;
static uint16_t g_eeprom_page_size = 0;

void EEPROM_Init(void) {
  g_eeprom_size = EEPROM_DetectSize();
  g_eeprom_page_size = EEPROM_GetPageSize();
  Log("EEPROM: %lu bytes, page %u bytes\n", g_eeprom_size, g_eeprom_page_size);
}

bool EEPROM_WaitReady(uint8_t device_addr, uint32_t timeout_ms) {
  uint64_t start = GetUptimeUs();
  while ((GetUptimeUs() - start) < (timeout_ms * 1000ULL)) {
    I2C_Start();
    int result = I2C_Write(device_addr);
    I2C_Stop();
    if (result == 0)
      return true;
    TIMER_DelayUs(100);
  }
  return false;
}

// Оптимизированное чтение с Sequential Read
// Автоматически определяет границы чипов и читает максимальными блоками
int EEPROM_ReadBufferSequential(uint32_t address, void *pBuffer,
                                uint16_t size) {
  if (size == 0)
    return 0;

  uint8_t *pData = (uint8_t *)pBuffer;
  address &= 0x3FFFF;

  while (size > 0) {
    // Определяем границу текущего чипа (64KB для большинства EEPROM)
    uint32_t chip_boundary =
        (address & 0x30000) + 0x10000; // Следующая граница 64KB
    uint32_t bytes_to_boundary = chip_boundary - address;
    uint16_t chunk_size = (size < bytes_to_boundary) ? size : bytes_to_boundary;

    // Адрес устройства с учетом старших битов адреса
    uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

    // Отправляем адрес для чтения
    I2C_Start();
    if (I2C_Write(IIC_ADD) != 0) {
      I2C_Stop();
      return -1;
    }
    if (I2C_Write((address >> 8) & 0xFF) != 0) {
      I2C_Stop();
      return -1;
    }
    if (I2C_Write(address & 0xFF) != 0) {
      I2C_Stop();
      return -1;
    }

    // Repeated START для чтения
    I2C_RepStart();
    if (I2C_Write(IIC_ADD | 0x01) != 0) {
      I2C_Stop();
      return -1;
    }

    // Sequential Read - читаем весь блок за раз
    I2C_ReadBuffer(pData, chunk_size);
    I2C_Stop();

    pData += chunk_size;
    address += chunk_size;
    size -= chunk_size;
  }

  return 0;
}

// Обновляем основную функцию чтения
int EEPROM_ReadBuffer(uint32_t address, void *pBuffer, uint16_t size) {
  return EEPROM_ReadBufferSequential(address, pBuffer, size);
}

/* int EEPROM_ReadBuffer(uint32_t address, void *pBuffer, uint16_t size) {
  if (size == 0)
    return 0;

  address &= 0x3FFFF;
  uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

  I2C_Start();
  if (I2C_Write(IIC_ADD) != 0) {
    I2C_Stop();
    return -1;
  }
  if (I2C_Write((address >> 8) & 0xFF) != 0) {
    I2C_Stop();
    return -1;
  }
  if (I2C_Write(address & 0xFF) != 0) {
    I2C_Stop();
    return -1;
  }

  I2C_RepStart();
  if (I2C_Write(IIC_ADD | 0x01) != 0) {
    I2C_Stop();
    return -1;
  }

  I2C_ReadBuffer(pBuffer, size);
  I2C_Stop();
  return 0;
} */

void EEPROM_WriteBuffer(uint32_t address, void *pBuffer, uint16_t size) {
  if (pBuffer == NULL || size == 0)
    return;

  uint16_t PAGE_SIZE = EEPROM_GetPageSize();
  address &= 0x3FFFF;

  while (size > 0) {
    uint16_t page_offset = address % PAGE_SIZE;
    uint16_t chunk_size =
        (size < (PAGE_SIZE - page_offset)) ? size : (PAGE_SIZE - page_offset);
    uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

    if (!EEPROM_WaitReady(IIC_ADD, 10))
      return;

    I2C_Start();
    if (I2C_Write(IIC_ADD) != 0) {
      I2C_Stop();
      return;
    }
    if (I2C_Write((address >> 8) & 0xFF) != 0) {
      I2C_Stop();
      return;
    }
    if (I2C_Write(address & 0xFF) != 0) {
      I2C_Stop();
      return;
    }

    // Пишем всю страницу за раз
    if (I2C_WriteBuffer(pBuffer, chunk_size) != 0) {
      I2C_Stop();
      return;
    }

    I2C_Stop();

    // Ждем завершения записи
    if (!EEPROM_WaitReady(IIC_ADD, 10))
      return;

    pBuffer += chunk_size;
    address += chunk_size;
    size -= chunk_size;
    gEepromWrite = true;
  }
}

/* void EEPROM_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint16_t size) {
  if (pBuffer == NULL || size == 0)
    return;

  uint16_t PAGE_SIZE = EEPROM_GetPageSize();
  address &= 0x3FFFF;

  while (size > 0) {
    uint16_t page_offset = address % PAGE_SIZE;
    uint16_t chunk_size =
        (size < (PAGE_SIZE - page_offset)) ? size : (PAGE_SIZE - page_offset);
    uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

    if (!EEPROM_WaitReady(IIC_ADD, 10))
      return;
    TIMER_DelayUs(100);

    I2C_Start();
    if (I2C_Write(IIC_ADD) != 0) {
      I2C_Stop();
      return;
    }
    if (I2C_Write((address >> 8) & 0xFF) != 0) {
      I2C_Stop();
      return;
    }
    if (I2C_Write(address & 0xFF) != 0) {
      I2C_Stop();
      return;
    }

    for (uint16_t i = 0; i < chunk_size; i++) {
      if (I2C_Write(pBuffer[i]) != 0) {
        I2C_Stop();
        return;
      }
    }

    I2C_Stop();
    SYS_DelayMs(15);

    if (!EEPROM_WaitReady(IIC_ADD, 50))
      return;

    pBuffer += chunk_size;
    address += chunk_size;
    size -= chunk_size;
    gEepromWrite = true;
  }
} */

void EEPROM_ClearPage(uint16_t page) {
  uint16_t PAGE_SIZE = EEPROM_GetPageSize();
  uint32_t address = page * PAGE_SIZE;
  uint8_t IIC_ADD = 0xA0 | ((address >> 15) & 0x0E);

  if (!EEPROM_WaitReady(IIC_ADD, 100))
    return;
  TIMER_DelayUs(100);

  I2C_Start();
  if (I2C_Write(IIC_ADD) != 0) {
    I2C_Stop();
    return;
  }
  if (I2C_Write((address >> 8) & 0xFF) != 0) {
    I2C_Stop();
    return;
  }
  if (I2C_Write(address & 0xFF) != 0) {
    I2C_Stop();
    return;
  }

  for (uint16_t i = 0; i < PAGE_SIZE; i++) {
    if (I2C_Write(0xFF) != 0) {
      I2C_Stop();
      return;
    }
  }

  I2C_Stop();
  SYS_DelayMs(10);
  gEepromWrite = true;
}

EEPROMType EEPROM_DetectType(void) {
  static uint8_t found_devices = 0;
  if (!found_devices) {
    for (uint8_t i = 0; i < 8; i++) {
      if (EEPROM_Detect(0xA0 | (i << 1)))
        found_devices++;
    }
  }

  switch (found_devices) {
  case 1:
    return EEPROM_BL24C64;
  case 2:
    return EEPROM_BL24C128;
  case 4:
    return EEPROM_M24M02;
  case 8:
    return EEPROM_BL24C512;
  default:
    return EEPROM_BL24C64;
  }
}

uint32_t EEPROM_DetectSize(void) { return EEPROM_SIZES[EEPROM_DetectType()]; }

uint16_t EEPROM_GetPageSize(void) { return PAGE_SIZES[EEPROM_DetectType()]; }

int EEPROM_ReadByte(uint32_t address, uint8_t *value) {
  return EEPROM_ReadBuffer(address, value, 1);
}

int EEPROM_WriteByte(uint32_t address, uint8_t value) {
  EEPROM_WriteBuffer(address, &value, 1);
  return 0;
}

bool EEPROM_Detect(uint8_t device_addr) {
  I2C_Start();
  int result = I2C_Write(device_addr);
  I2C_Stop();
  return (result == 0);
}

bool EEPROM_Test(uint32_t test_addr) {
  uint8_t test_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
  uint8_t read_data[8] = {0};

  EEPROM_WriteBuffer(test_addr, test_data, sizeof(test_data));
  if (EEPROM_ReadBuffer(test_addr, read_data, sizeof(read_data)) != 0)
    return false;

  return (memcmp(test_data, read_data, sizeof(test_data)) == 0);
}

#include "uart.h"
void EEPROM_TestReadSpeed() {
  uint8_t buf[256];
  Log("-- EEPROM TEST START --");
  for (uint32_t i = 0; i < g_eeprom_size; i += g_eeprom_page_size) {
    EEPROM_ReadBuffer(i, buf, g_eeprom_page_size);
  }
  Log("-- EEPROM TEST END --");
}

void EEPROM_ScanBus(void) {
  printf("\nI2C Scan:\n     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
  for (uint8_t row = 0; row < 8; row++) {
    printf("%02X: ", row * 16);
    for (uint8_t col = 0; col < 16; col++) {
      uint8_t addr = (row * 16 + col) << 1;
      if (addr < 0x10 || addr > 0xF0) {
        printf("   ");
        continue;
      }

      I2C_Start();
      int result = I2C_Write(addr);
      I2C_Stop();

      printf(result == 0 ? "%02X " : "-- ", addr);
      TIMER_DelayUs(100);
    }
    printf("\n");
  }
}

void EEPROM_TestReadSpeedFast() {
  static uint8_t buf[1024]; // Буфер 1KB
  uint32_t total_size = g_eeprom_size;

  printf("Testing I2C raw speed...\n");
  uint64_t start = GetUptimeUs();

  I2C_Start();
  I2C_Write(0xA0);
  I2C_Write(0x00);
  I2C_Write(0x00);
  I2C_RepStart();
  I2C_Write(0xA1);

  for (int i = 0; i < 1000; i++) {
    I2C_Read(false);
  }
  I2C_Read(true);
  I2C_Stop();

  uint64_t elapsed = GetUptimeUs() - start;
  printf("1000 bytes in %lu us = %.2f KB/s\n", (uint32_t)elapsed,
         1000000.0f / elapsed);

  Log("-- EEPROM FAST TEST START --");

  // Читаем по границам чипов (64KB)
  for (uint32_t base_addr = 0; base_addr < total_size; base_addr += 65536) {
    uint32_t chip_size =
        ((base_addr + 65536) > total_size) ? (total_size - base_addr) : 65536;

    uint8_t IIC_ADD = 0xA0 | ((base_addr >> 15) & 0x0E);

    // Один START для всего чипа
    I2C_Start();
    I2C_Write(IIC_ADD);
    I2C_Write((base_addr >> 8) & 0xFF);
    I2C_Write(base_addr & 0xFF);
    I2C_RepStart();
    I2C_Write(IIC_ADD | 0x01);

    // Читаем весь чип последовательно
    uint32_t remaining = chip_size;
    while (remaining > 0) {
      uint16_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
      I2C_ReadBuffer(buf, chunk);
      remaining -= chunk;
    }

    I2C_Stop();
  }

  Log("-- EEPROM FAST TEST END --");
}
