#include "eeprom.h"
#include "at24c256.h"
#include "hw.h"

AT24c256_T *at24;
#define AT24_I2C_ADDR 0xA0

static int base_addr;

int EE_Init(void)
{
    at24 = Create_AT24C256();
    i_assert(at24);
    
    at24->Set_I2C_Addr(at24, sw_i2c1, AT24_I2C_ADDR);
    return 0;    
}

void EE_SetBase(uint16_t addr)
{
    base_addr = addr;
}

int EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data)
{
    int rt = at24->Read_Word(at24, base_addr + (VirtAddress-base_addr)*2, Data);
    if (rt == 2) {
        return 0;
    } else if (rt == 0) {
        return -1;
    } else {
        return -rt;
    }
}
int EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    int rt = at24->Write_Word(at24, base_addr + (VirtAddress-base_addr)*2, Data);

    if (rt == 2) {
        return 0;
    } else if (rt == 0) {
        return -1;
    } else {
        return -rt;
    }
}

int EE_ReadVariable32(uint16_t VirtAddress, uint32_t* Data)
{
    uint16_t *p = (uint16_t *)Data;
    int i, status=0;

    for (i=0; i<2; i++) {
        status |= EE_ReadVariable(VirtAddress++, p++);
    }
    
    return status;
}

int EE_WriteBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len)
{
    return at24->Write_Bytes(at24, VirtAddress, Data, len);
}

int EE_ReadBytes(uint16_t VirtAddress, uint8_t *Data, uint16_t len)
{
    return at24->Read_Bytes(at24, VirtAddress, Data, len);
}

