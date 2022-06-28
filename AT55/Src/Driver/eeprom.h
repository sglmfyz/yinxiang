#ifndef __EEPROM_H__
#define __EEPROM_H__

#include "common.h"

#define NB_OF_VAR             ((uint8_t)0x20)


int EE_Init(void);
int EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data);
int EE_WriteVariable(uint16_t VirtAddress, uint16_t Data);
int EE_ReadVariable32(uint16_t VirtAddress, uint32_t* Data);
void EE_SetBase(uint16_t base);

int EE_WriteBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len);
int EE_ReadBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len);

#endif

