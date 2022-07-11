#include <stdarg.h>

#include "i2c.h"
#include "util.h"
#include "gpio.h"
#include "heap.h"
#include "log.h"

#define SW_I2C_DELAY_DEF 4
#define ACK_TIMEOUT_DEF 100

#define _scl_low(i2c) GPIO_Write_Low(i2c->clk_io, i2c->clk_pin)
#define _scl_hi(i2c) GPIO_Write_Hi(i2c->clk_io, i2c->clk_pin)
#define _sda_low(i2c) GPIO_Write_Low(i2c->data_io, i2c->data_pin)
#define _sda_hi(i2c) GPIO_Write_Hi(i2c->data_io, i2c->data_pin)
#define _sda_state(i2c) GPIO_Read(i2c->data_io, i2c->data_pin)


typedef struct {
    I2C_T public;
    gpio_type *clk_io, *data_io;
    uint16_t clk_pin, data_pin;
    uint16_t delay;
} sw_i2c_info_t;


static inline void _i2c_delay(sw_i2c_info_t *i2c)
{
    Delay_us(i2c->delay);
}

static int _i2c_start(I2C_T *this)
{ 
    sw_i2c_info_t *i2c;

    i2c = container_of(this, sw_i2c_info_t, public);
    
    _sda_hi(i2c); 
    _scl_hi(i2c); 
    _i2c_delay(i2c);    

    if(!_sda_state(i2c)) {
        return SI2C_BUS_BUSY;
    }
    _sda_low(i2c);
    _i2c_delay(i2c);
      
    _scl_low(i2c);  
    _i2c_delay(i2c); 

    if(_sda_state(i2c)) {
        return SI2C_BUS_ERROR;
    } 
     
    return SI2C_READY;
}

static void _i2c_stop(I2C_T *this)
{
    sw_i2c_info_t *i2c;

    i2c = container_of(this, sw_i2c_info_t, public);
    _sda_low(i2c); 
    _i2c_delay(i2c);

    _scl_hi(i2c); 
    _i2c_delay(i2c);    

    _sda_hi(i2c);
    _i2c_delay(i2c);
}

static void _i2c_send_ack(I2C_T *this)
{
    sw_i2c_info_t *i2c;
    
    i2c = container_of(this, sw_i2c_info_t, public);
    _sda_low(i2c);
    _i2c_delay(i2c);
    _scl_hi(i2c);
    _i2c_delay(i2c);
    _scl_low(i2c); 
    _i2c_delay(i2c); 
    _sda_hi(i2c);
    
}

static void _i2c_send_nack(I2C_T *this)
{
    sw_i2c_info_t *i2c;
        
    i2c = container_of(this, sw_i2c_info_t, public);
    _sda_hi(i2c);
    _i2c_delay(i2c);
    _scl_hi(i2c);
    _i2c_delay(i2c);
    _scl_low(i2c); 
    _i2c_delay(i2c);

    
}

static int _i2c_send_byte(I2C_T *this, uint8_t Data, uint16_t ack_timeout_us)
{
    int i;
    uint32_t m;
    sw_i2c_info_t *i2c;
        
    i2c = container_of(this, sw_i2c_info_t, public);
 
    _scl_low(i2c);
    _i2c_delay(i2c);

    
    for(i=0;i<8;i++) {  
        if(Data&0x80) {
            _sda_hi(i2c);
        } else {
            _sda_low(i2c);
        } 
        Data <<= 1;
        _i2c_delay(i2c);
        //---数据建立保持一定延时----

        //----产生一个上升沿[正脉冲] 
        _scl_hi(i2c);
        _i2c_delay(i2c);
        _scl_low(i2c);
        _i2c_delay(i2c);//延时,防止SCL还没变成低时改变SDA,从而产生START/STOP信号
    }
    //接收从机的应答 
    _sda_hi(i2c); 
    _i2c_delay(i2c);
    _scl_hi(i2c);
    _i2c_delay(i2c);   

    m = Millis();

    do {
        if(!_sda_state(i2c)) {
            _scl_low(i2c);
            _sda_hi(i2c);
            return SI2C_ACK;
        }
    } while (Millis() - m <= ack_timeout_us);
    
    _scl_low(i2c);
    _sda_hi(i2c);
    return SI2C_NACK;
}

/* --------------------------------------------------------------------------*/
/** 
 * @Brief:  SI2C_ReceiveByte 
 * 
 * @Returns:   
 */
/* --------------------------------------------------------------------------*/
static uint8_t _i2c_recv_byte(I2C_T *this)
{
    int i;
    uint8_t Dat;
    sw_i2c_info_t *i2c;
        
    i2c = container_of(this, sw_i2c_info_t, public);
    
    _sda_hi(i2c);
    _scl_low(i2c); 

    Dat=0;
    
    for(i=0;i<8;i++) {
        _scl_hi(i2c);//产生时钟上升沿[正脉冲],让从机准备好数据 
        _i2c_delay(i2c); 
        Dat <<= 1;
        if(_sda_state(i2c)) {
            Dat |= 0x01; 
        }   
        _scl_low(i2c);//准备好再次接收数据  
        _i2c_delay(i2c);//等待数据准备好         
    }
    return Dat;
}


static int _set_delay(I2C_T *this, uint16_t us)
{
    sw_i2c_info_t *info;

    info = container_of(this, sw_i2c_info_t, public);
    info->delay = us;

    return 0;
}


static int _mem_write(I2C_T *this, uint8_t DevAddress, uint8_t *MemAddress, uint16_t AddrSize, uint8_t *pData, uint16_t Size)
{  
    //devaddress=i2c地址。mem地址-->为&addr ff0? data为传入数据地址，地址发过去
    int i, status;
    uint8_t tmp[2];

    i_assert(AddrSize == 1 || AddrSize == 2);
    status = _i2c_start(this);
    if_error_goto(status == SI2C_READY, end);

    /*Write Dev address and Memaddress*/
    status = _i2c_send_byte(this, DevAddress, ACK_TIMEOUT_DEF);
    if_error_goto(status == SI2C_ACK, end);

    if (AddrSize == 2) {
        tmp[0] = MemAddress[1];
        tmp[1] = MemAddress[0];
    printf("MemAddress1=%d  0=%d    \n",MemAddress[1],MemAddress[0]);
    } else {
        tmp[0] = MemAddress[0];
     printf("MemAddress  0=%d    \n",MemAddress[0]);
    }
    
    for (i=0; i<AddrSize; i++) {
        status = _i2c_send_byte(this, tmp[i], ACK_TIMEOUT_DEF);
        if_error_goto(status == SI2C_ACK, end);
    }

    for (i=0; i<Size; i++) {
        status = _i2c_send_byte(this, pData[i], ACK_TIMEOUT_DEF);
        if_error_goto(status == SI2C_ACK, end);
    }
    status = 0;
end:
    _i2c_stop(this);

    return -status;
}


static int _mem_read(I2C_T *this, uint8_t DevAddress, uint8_t *MemAddress, uint16_t AddrSize, uint8_t *pData, uint16_t Size)
{
    int i, status;
    uint8_t tmp[2];
    
    i_assert(AddrSize == 1 || AddrSize == 2);

    status = _i2c_start(this);
    if_error_goto(status == SI2C_READY, end);

    /*Write Dev address and Memaddress*/
    
    status = _i2c_send_byte(this, DevAddress, ACK_TIMEOUT_DEF);
    if_error_goto(status == SI2C_ACK, end);

    if (AddrSize == 2) {
        tmp[0] = MemAddress[1];
        tmp[1] = MemAddress[0];
    } else {
        tmp[0] = MemAddress[0];
    }

    for (i=0; i<AddrSize; i++) {
        status = _i2c_send_byte(this, tmp[i], ACK_TIMEOUT_DEF);
        if_error_goto(status == SI2C_ACK, end);
    }
    
    /*Repeat Start*/
    status = _i2c_start(this);
    if_error_goto(status == SI2C_READY, end);
    status = _i2c_send_byte(this, DevAddress|1, ACK_TIMEOUT_DEF);
    if_error_goto(status == SI2C_ACK, end);
    
    for (i=0; i<Size; i++) {
        pData[i] = _i2c_recv_byte(this);

        if (i != Size-1) {
            _i2c_send_ack(this);
        } else {
            _i2c_send_nack(this);
        }
    }
    status = 0;
end:
    _i2c_stop(this);

    return -status;
}

static int _master_transmit(I2C_T *this, uint8_t DevAddress, uint8_t *pData, uint16_t Size)
{
    int i, status;

    status = _i2c_start(this);
    if_error_goto(status == SI2C_READY, end);

    status = _i2c_send_byte(this, DevAddress, ACK_TIMEOUT_DEF);
    if_error_goto(status == SI2C_ACK, end);

    for (i=0; i<Size; i++) {
        status = _i2c_send_byte(this, pData[i], ACK_TIMEOUT_DEF);
        if_error_goto(status == SI2C_ACK, end);
    }
    status = 0;
end:
    
    _i2c_stop(this);
    return -status;
}
static int _master_receive(I2C_T *this, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    int i, status;

    status = _i2c_start(this);
    if_error_goto(status == SI2C_READY, end);

    status = _i2c_send_byte(this, DevAddress | 1, ACK_TIMEOUT_DEF);
    //if_error_goto(status == SI2C_ACK, end);

    for (i=0; i<Size; i++) {
        pData[i] = _i2c_recv_byte(this);
        if (i != Size-1) {
            _i2c_send_ack(this);
        } else {
            _i2c_send_nack(this);
        }
    }
    status = 0;
end:
    
    _i2c_stop(this);

    return -status;
}

I2C_T *Create_SW_I2C(gpio_type       *clk_io, uint16_t clk_pin, gpio_type *data_io, uint16_t data_pin)
{
    sw_i2c_info_t *info;
    I2C_T *public;

    zmalloc(info, sizeof(sw_i2c_info_t));
    info->delay = SW_I2C_DELAY_DEF;
    info->clk_io = clk_io;
    info->clk_pin = clk_pin;
    info->data_io = data_io;
    info->data_pin = data_pin;
    public = &info->public;

    public->Set_Delay = _set_delay;
    
    public->Start = _i2c_start;
    public->Stop = _i2c_stop;
    public->Send_Ack = _i2c_send_ack;
    public->Send_Nack = _i2c_send_nack;
    public->Send_Byte = _i2c_send_byte;
    public->Recv_Byte = _i2c_recv_byte;

    
    public->Mem_Read = _mem_read;
    public->Mem_Write = _mem_write;
    public->Master_Receive = _master_receive;
    public->Master_Transmit = _master_transmit;
    
    _sda_hi(info);
    _scl_hi(info); 
    return &info->public;  
}

