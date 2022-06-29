#include "common.h"
#include "shell.h"
#include "at24c256.h"
#include "i2c.h"
#include "heap.h"
#include "log.h"

#include "util.h"
#define MAX_WRITE_BYTES 64
#define MAX_READ_BYTES 64
#define WRITE_CYCLE 6

typedef struct {
    AT24c256_T public;
    I2C_T *i2c;
    uint8_t addr;
    uint32_t last_write;
} at24_info_t;

void _set_i2c_addr(AT24c256_T *this, I2C_T *i2c, uint8_t addr)
{
    at24_info_t *info;

    info = container_of(this, at24_info_t, public);

    info->i2c = i2c;
    info->addr = addr;
}
static int _write_bytes(AT24c256_T *this, uint16_t addr, uint8_t *data, uint16_t len)
{
    at24_info_t *info;
    int l, left, status;
    uint32_t now;
    int end;

    info = container_of(this, at24_info_t, public);
    left = len;
    
    while (left > 0) {
        end = align(addr+1, 64);
        if (left > end - addr) {
            l = end - addr;
        } else {
            l = left;
        }
        //syslog_debug("Write addr %d, end:%d, l %d\n", addr, end, l);

        now = Micros();
        if (now - info->last_write <= WRITE_CYCLE) {
            Delay_ms(WRITE_CYCLE - (now - info->last_write));
        }
        status = info->i2c->Mem_Write(info->i2c, info->addr, (uint8_t *)&addr, 2, data, l);
        if (status == 0) {
            left -= l;
            addr += l;
            data += l;
            info->last_write = Micros();
        } else {
            break;
        }
    }
    //
    return len - left;
}

static int _write_word(AT24c256_T *this, uint16_t addr, uint16_t data)
{
    return _write_bytes(this, addr, (uint8_t *)&data, 2);
}

static int _read_bytes(AT24c256_T *this, uint16_t addr, uint8_t *data, uint16_t len)
{
    at24_info_t *info;
    int status;
    int l, left;
    uint32_t now;
    info = container_of(this, at24_info_t, public);
    left = len;

    now = Micros();
    if (now - info->last_write <= WRITE_CYCLE) {
        Delay_ms(WRITE_CYCLE - (now - info->last_write));
    }
    
    while (left > 0) {
        if (left > MAX_READ_BYTES) {
            l = MAX_READ_BYTES;
        } else {
            l = left;
        }        

        status = info->i2c->Mem_Read(info->i2c, info->addr, (uint8_t *)&addr, 2, data, l);
        if (status == 0) {
            left -= l;
            addr += l;
            data += l;
        } else {
            break;
        }
    }
    return len - left;
}

static int _read_word(AT24c256_T *this, uint16_t addr, uint16_t *data)
{
    return _read_bytes(this, addr, (uint8_t *)data, 2);
}

AT24c256_T *Create_AT24C256()
{
    AT24c256_T *public;
    at24_info_t *info;
    
    zmalloc(info, sizeof(*info));
    public = &info->public;
    public->Set_I2C_Addr = _set_i2c_addr;
    public->Write_Bytes = _write_bytes;
    public->Read_Bytes = _read_bytes;
    public->Write_Word = _write_word;
    public->Read_Word = _read_word;

    return public;
}
