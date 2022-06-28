#ifndef __HEAP_H__
#define __HEAP_H__

#include "common.h"
/**
  * @brief  堆分配函数，从freertos的heap1而来
  * @param  size:    待分配内存大小

  * @retval 分配好的指针
  */
void *pMalloc(int size);

/**
  * @brief  堆剩余空间
  * @param  None
  * @retval 堆剩余的字节数
  */
uint32_t heap_free_bytes();

/**
  * @brief  堆已用字节数
  * @param  None
  * @retval 堆已使用的字节数
  */
uint32_t heap_used_bytes();

/**
  * @brief  堆空间大小
  * @param  None
  * @retval 堆空间字节数
  */
uint32_t heap_total_bytes();



#endif
