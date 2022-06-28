#include "common.h"
//#include "update.h"
/**
  * @brief  initialize Delay function   
  * @param  None
  * @retval None
  */		   

/**
  * @brief  Inserts a delay time.
  * @param  nus: specifies the delay time length, in microsecond.
  * @retval None
  */


static __IO uint32_t uwTick;
static __IO uint32_t uwMills;
static uint16_t last_us;
static tmr_type *usTime;

void Sys_IncTick(void)
{
  uint32_t cnt;

  uwTick++;
  cnt = tmr_counter_value_get(usTime);
  uwMills += (uint16_t)(cnt - last_us);
  last_us = cnt;
}


void Delay_SetTMR(tmr_type *tim)
{
    /*Config Systick*/
    usTime = tim;
}

void Delay_us(u32 us)
{
    volatile uint16_t temp;

    uint16_t cnt = usTime->cval;

    do {
        temp = usTime->cval;
    } while((uint16_t)(temp - cnt) < us);
}

/**
  * @brief  Inserts a delay time.
  * @param  nms: specifies the delay time length, in milliseconds.
  * @retval None
  */
void Delay_ms(u16 ms)
{
    uint32_t now, last;

    last = Micros();
    do {
        now = Micros();
    } while   (now - last <= ms);
}

/**
  * @brief  Inserts a delay time.
  * @param  sec: specifies the delay time length, in seconds.
  * @retval None
  */
void Delay_sec(u16 sec)
{
  u16 i;
  for(i=0; i<sec; i++)
  {
    Delay_ms(500);
    Delay_ms(500);
  }
}

uint32_t Micros()
{
    return uwTick * (1000/HZ);
}

uint32_t Millis()
{
    uint32_t u =  uwMills;
    uint16_t cnt;

    cnt = usTime->cval;
    return (uint32_t)(u + (uint16_t)(cnt - last_us));
}

// 调用时，tmp首先要初始化为0, 且tmp在每次调用进来时需要保持上次的值，
// 所以，该值为全局或static类型
int anti_jitter(uint8_t dir, uint8_t jitter_counter, uint8_t current, uint8_t *tmp)
{
    int rt = current;

    if (jitter_counter == 0) {
        jitter_counter = 1;
    }
    
    if (current == 0) {
        if (dir > 0) {
            (*tmp)++;
        } else {
            *tmp = 0;
        }
        if (*tmp >= jitter_counter) {
            rt = 1;
            *tmp = 0;
        }
    } else if (current == 1) {
        if (dir == 0) {
            (*tmp)++;
        } else {
            *tmp = 0;
        }
        if (*tmp >= jitter_counter) {
            rt = 0;
            *tmp = 0;
        }
    }
    return rt;
}

uint32_t htonl(uint32_t v)
{
    return ((v & 0xff) << 24) | (((v >> 8) & 0xff) << 16) |
           (((v >> 16) & 0xff) << 8) | ((v >> 24) & 0xff);
}
uint32_t ntohl(uint32_t v)
{
    return ((v & 0xff) << 24) | (((v >> 8) & 0xff) << 16) |
           (((v >> 16) & 0xff) << 8) | ((v >> 24) & 0xff);
}

uint16_t htons(uint16_t v)
{
    return ((v & 0xff) << 8) | ((v >> 8) & 0xff);
}

uint16_t ntohs(uint16_t v)
{
    return ((v & 0xff) << 8) | ((v >> 8) & 0xff);
}


char *strnstr(const char* s1, const char* s2, size_t n)
{
    const char *p;
    size_t len = strlen(s2);
    
    if (len == 0) {
        return (char *)s1;
    }

    for (p = s1; *p && (p + len <= s1 + n); p++) {
        if (strncmp(p, s2, len) == 0) {
            return (char *)p;
        }
    }
    return NULL;
}

uint16_t common_crc16(uint8_t *buf, uint16_t size)
{
    uint16_t temp, flag;
    int i, j;
    
    temp = 0xFFFF;
    for (i = 0; i < size; i++) {
        temp = temp ^ buf[i];
        for ( j = 1; j <= 8; j++) {
            flag = temp & 0x0001;
            temp >>=1;
            if (flag) {
                temp ^= 0xA001;
            }
        }
    }
    temp &= 0xFFFF;
    return temp;
}

uint16_t common_crc4(uint8_t *buf, uint8_t size)
{
	uint8_t ucTemp;
	uint16_t wValue;
	uint16_t crc_tbl[16]={0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
	0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef}; 

	wValue=0;
	while(size--!=0)
	{
        ucTemp=((wValue>>8))>>4;
        wValue<<=4;
        wValue^=crc_tbl[ucTemp^((*buf)>>4)];
        ucTemp=((wValue>>8))>>4;
        wValue<<=4;
        wValue^=crc_tbl[ucTemp^((*buf)&0x0f)];
        buf++;
	}
    
	return wValue;
}


void dump_bin(uint8_t *data, int num)
{
    int i;
    for (i=0; i<num; i++) {
        if (i != 0 && i % 16 ==  0) {
            printf("\n");
        }
        printf("%02x ", data[i]);
    }
    printf("\n");    
}

uint32_t get_tim_freq(tmr_type *tim) 
{
    crm_clocks_freq_type freq;

    crm_clocks_freq_get(&freq);
    
    if (tim == TMR17 || tim == TMR16 || tim == TMR15 || tim == TMR1) {
        return freq.apb2_freq;
    } else if (tim == TMR14 || tim == TMR6 || tim == TMR3) {
        return freq.apb1_freq;
    }
    return 0;
}


