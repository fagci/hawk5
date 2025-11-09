#include "eeprom.h"
#include "../external/printf/printf.h"
#include "i2c.h"
#include "system.h"
#include "systick.h"
#include <string.h>

bool gEepromWrite = false;

static uint32_t g_eeprom_size = 0;
static uint16_t g_eeprom_page_size = 0;

void EEPROM_Init(void) {
  g_eeprom_size = EEPROM_DetectSize();
  g_eeprom_page_size = EEPROM_GetPageSize();
  printf("EEPROM: %lu bytes, page %u bytes\n", g_eeprom_size,
         g_eeprom_page_size);
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

int EEPROM_ReadBuffer(uint32_t address, void *pBuffer, uint16_t size) {
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
}

void EEPROM_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint16_t size) {
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
}

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
