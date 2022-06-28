#ifndef __UTIL_H__
#define __UTIL_H__

#include <stddef.h>

void Sys_IncTick(void);
void Delay_SetTMR(tmr_type *tim);

void Delay_us(u32 nus);
void Delay_ms(u16 nms);

void Delay_sec(u16 sec);
uint32_t Micros();
uint32_t Millis();
/**
  * @brief  内存查找函数
  * @param  start: 开始指针
  * @param  v: key
  * @param  len： 查找长度
  * @retval NULL: 未找到
            其他值: 查找到的指针
  */
void *memrchr(void *start, uint8_t v, uint16_t len);

int anti_jitter(uint8_t dir, uint8_t jitter_counter, uint8_t current, uint8_t *tmp);
char *keys_search(char *data, char *key, char sep, int size);
int msg_get_int(char *data, char start, char sep, int field, int len, int *v);
int str_get_int(char *data, int *val);
void dump_bin(uint8_t *data, int num);

uint16_t htons(uint16_t v);
uint16_t ntohs(uint16_t v);
uint32_t htonl(uint32_t v);
uint32_t ntohl(uint32_t v);

char *strnstr(const char* s1, const char* s2, size_t n);

uint16_t common_crc16(uint8_t *buf, uint16_t size);
uint16_t common_crc4(uint8_t *buf, uint8_t size);
uint32_t get_tim_freq(tmr_type *tim);

#endif
