#ifndef DRIVER_EEPROM_H
#define DRIVER_EEPROM_H

#include "../settings.h"
#include <stdbool.h>
#include <stdint.h>

extern bool gEepromWrite;

int EEPROM_ReadBuffer(uint32_t Address, void *pBuffer, uint16_t Size);
void EEPROM_WriteBuffer(uint32_t Address, uint8_t *pBuffer, uint16_t Size);
void EEPROM_ClearPage(uint16_t page);
void EEPROM_ScanBus(void);
bool EEPROM_Test(uint32_t test_addr);
bool EEPROM_Detect(uint8_t device_addr);
uint16_t EEPROM_GetPageSize(void);
uint32_t EEPROM_DetectSize(void);
void EEPROM_Init(void);
EEPROMType EEPROM_DetectType(void);
void EEPROM_TestReadSpeed();
void EEPROM_TestReadSpeedFast();

#endif
