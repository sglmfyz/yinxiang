#include "common.h"
#include "eeprom.h"
#include "hw.h"

#if AT24_EEPROM == 1

#include "at24c256.h"

#define AT24_I2C i2c2
#define AT24_I2C_ADDR 0xA0
AT24c256_T *AT24;


int EE_Init(void)
{    
    AT24 = Create_AT24C256();
    i_assert(AT24);
    AT24->Set_I2C_Addr(AT24, AT24_I2C, AT24_I2C_ADDR);

    return 0;
}
int EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data)
{
    return AT24->Read_Word(AT24, VirtAddress, Data);
}
int EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    return AT24->Write_Word(AT24, VirtAddress, Data);
}

int EE_WriteBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len)
{
    return AT24->Write_Bytes(AT24, VirtAddress, Data, len);
}

int EE_ReadBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len)
{
    return AT24->Read_Bytes(AT24, VirtAddress, Data, len);
}


#else
int EE_Init(void)
{
    return 0;
}
int EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data)
{
    return -1;
}
int EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    return -1;
}
int EE_WriteBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len)
{
    return -1;
}
int EE_ReadBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len)
{
    return -1;
}

#endif

