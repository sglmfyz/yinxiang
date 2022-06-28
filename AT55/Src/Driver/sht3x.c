#include "sht3x.h"
#include "heap.h"
#include "log.h"

#define POLYNOMIAL  0x131 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

typedef struct {
    SHT3X_T public;
    I2C_T *i2c;
    uint8_t addr;
} sht3x_info_t;


static uint8_t _sht3x_crc(uint8_t data[], uint8_t nbrOfBytes)
{
  uint8_t bit;        // bit mask
  uint8_t crc = 0xFF; // calculated checksum
  uint8_t byteCtr;    // byte counter
  
  // calculates 8-Bit checksum with given polynomial
  for(byteCtr = 0; byteCtr < nbrOfBytes; byteCtr++)
  {
    crc ^= (data[byteCtr]);
    for(bit = 8; bit > 0; --bit)
    {
      if(crc & 0x80) crc = (crc << 1) ^ POLYNOMIAL;
      else           crc = (crc << 1);
    }
  }
  
  return crc;
}

//-----------------------------------------------------------------------------
static etSht3xError _sht3x_checkcrc(uint8_t data[], uint8_t nbrOfBytes, uint8_t checksum)
{
  uint8_t crc;     // calculated checksum
  
  // calculates 8-Bit checksum
  crc = _sht3x_crc(data, nbrOfBytes);
  
  // verify checksum
  if(crc != checksum) return CHECKSUM_ERROR;
  else                return NO_ERROR;
}


/*static int _sht3x_read_word(sht3x_info_t *info, uint8_t addr, uint16_t cmd, uint16_t *word)
{
    uint8_t data[3];
    int status;

    cmd = SWAP_WORD(cmd);
    status = info->i2c->Mem_Read(info->i2c, addr, (uint8_t *)&cmd, sizeof(cmd), (uint8_t *)data, sizeof(data));

    if (status == 0) {
        status = _sht3x_checkcrc(data, 2, data[2]);
        *word = (data[0] << 8) | data[1];
    }
    return status;
}*/

static int _sht3x_read_2words(sht3x_info_t *info, uint8_t addr, uint16_t cmd, uint32_t *words)
{
    uint8_t data[6];
    int status;
    
    status = info->i2c->Mem_Read(info->i2c, addr, (uint8_t *)&cmd, sizeof(cmd), (uint8_t *)data, sizeof(data));

    if (status == 0) {
        status = _sht3x_checkcrc(data, 2, data[2]);
        status |= _sht3x_checkcrc(data+3, 2, data[5]);
        *words = (data[0] << 24) | (data[1] << 16) | (data[3] << 8) | data[4];
    }
    return status;
}

static int _sht3x_write_cmd(sht3x_info_t *info, uint8_t addr, uint16_t cmd)
{
    cmd = SWAP_WORD(cmd);
    return info->i2c->Master_Transmit(info->i2c, addr, (uint8_t *)&cmd, sizeof(cmd));
}

int _start_period_measure(SHT3X_T *this, etRepeatability repeatability,
                                      etFrequency frequency)
{
    sht3x_info_t *info;
    int status;
    uint16_t cmd;

    info = container_of(this, sht3x_info_t, public);
    status = 0;
    
    switch(repeatability)
    {
        case REPEATAB_LOW: // low repeatability
            switch(frequency)
            {
              case FREQUENCY_HZ5: 
                  cmd = CMD_MEAS_PERI_05_L; 
                  break;          
              case FREQUENCY_1HZ: 
                  cmd = CMD_MEAS_PERI_1_L; 
                  break;                      
              case FREQUENCY_2HZ: 
                  cmd = CMD_MEAS_PERI_2_L; 
                  break;          
              case FREQUENCY_4HZ: 
                  cmd = CMD_MEAS_PERI_4_L; 
                  break;          
              case FREQUENCY_10HZ: 
                  cmd = CMD_MEAS_PERI_10_L; 
                  break;          
              default: 
                  status = -PARM_ERROR; 
                  break;
            }
            break;
    
        case REPEATAB_MEDIUM: // medium repeatability
            switch(frequency)
            {
                case FREQUENCY_HZ5: 
                    cmd = CMD_MEAS_PERI_05_M; 
                    break;
                case FREQUENCY_1HZ: 
                    cmd = CMD_MEAS_PERI_1_M; 
                    break;        
                case FREQUENCY_2HZ: 
                    cmd = CMD_MEAS_PERI_2_M; 
                    break;        
                case FREQUENCY_4HZ: 
                    cmd = CMD_MEAS_PERI_4_M; 
                    break;      
                case FREQUENCY_10HZ: 
                    cmd = CMD_MEAS_PERI_10_M; 
                    break;
                default: 
                    status = -PARM_ERROR; 
                    break;
            }
            break;
    
        case REPEATAB_HIGH: // high repeatability
            switch(frequency)
            {
                case FREQUENCY_HZ5: 
                    cmd = CMD_MEAS_PERI_05_H;
                    break;
                case FREQUENCY_1HZ: 
                    cmd =  CMD_MEAS_PERI_1_H;
                    break;
                case FREQUENCY_2HZ:
                    cmd =  CMD_MEAS_PERI_2_H;
                    break;
                case FREQUENCY_4HZ:
                    cmd = CMD_MEAS_PERI_4_H;
                    break;
                case FREQUENCY_10HZ:
                    cmd = CMD_MEAS_PERI_10_H;
                    break;
                default:
                    status = -PARM_ERROR;
                    break;
            }
            break;
        
        default:
            status = -PARM_ERROR;
            break;
    }
    if (status != 0) {
        syslog_error("Start Period Measure error %d\n", status);
        return status;
    } else {
        status = _sht3x_write_cmd(info, info->addr, cmd);
        return status;
    }
}

static int _read_measure_buffer(SHT3X_T *this, 
                                    float* temperature, float* humidity)
{
    sht3x_info_t *info;
    uint32_t words;
    uint16_t rt, rh;
    int status;

    info = container_of(this, sht3x_info_t, public);
    status = _sht3x_read_2words(info, info->addr, CMD_FETCH_DATA, &words);

    if (status == 0) {
        rt = words >> 16;
        rh = words & 0xffff;
        *temperature = 175.0f * (float)rt / 65535.0f - 45.0f;;
        *humidity = 100.0f * (float)rh / 65535.0f;
    }
    return status;
}
static void _set_i2c_addr(SHT3X_T *this, I2C_T *i2c, uint8_t addr)
{
    sht3x_info_t *info;
    
    info = container_of(this, sht3x_info_t, public);
    info->i2c = i2c;
    //info->i2c->Set_Delay(info->i2c, 15);
    info->addr = addr;
}

SHT3X_T *Create_SHT3X(I2C_T *i2c)
{
    sht3x_info_t *info;
    SHT3X_T *public;

    
    zmalloc(info, sizeof(sht3x_info_t));

    info->i2c = i2c;
    public = &info->public;

    public->Set_I2C_Addr = _set_i2c_addr;
    public->Start_Periodic_Measurment = _start_period_measure;
    public->Read_Measurement_Buffer = _read_measure_buffer;
    return &info->public;
}



