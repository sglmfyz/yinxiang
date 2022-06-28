#ifndef __COMMON_H__
#define __COMMON_H__
#include "code.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "at32f421.h"

#include "config.h"
#include "util.h"

#define BOARD_DEBUG 0

#define bit(v) (1<<v)
#define bit_is_set(v, bit) ((v) & (1<<(bit)))
#define bit_is_clr(v, bit) (!bit_is_set((v), bit))
#define bit_set(v, bit) (v = ((v) | (1<<(bit))))
#define bit_clr(v, bit) (v = ((v) & ~(1<<(bit))))
#define bit_xor(v, bit) (v = ((v) ^ (1<<(bit))))
#define ROUND(x)  ((x)<0 ? ((double)(x) - 0.5f):((double)(x) + 0.5f))
#define ABS(x)  ((x) < 0 ? -(x):(x))
#define SWAP_WORD(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff))
#define word(hi, low) (uint16_t)(((hi & 0xff) << 8) | (low & 0xff))
#define high_byte(word) ((word >> 8) & 0xff)
#define low_byte(word) (word & 0xff)

#define high_word(word) ((word >> 16) & 0xffff)
#define low_word(word) (word & 0xffff)

#define align(x, a) (((x) + (a) - 1) & ~((a) - 1))

#define max(x, y) ((x) >= (y) ? (x) : (y))
#define min(x, y) ((x) >= (y) ? (y) : (x))


#ifndef offsetof
  #define offsetof(type, member) ( (unsigned long) & ((type*)0) -> member )
#endif
#define container_of(ptr, type, member) (type *)( (char *)ptr - offsetof(type,member) )

#define malloc pMalloc

#define if_error_goto(statement, err)    do {        \
    if (!(statement)) {                                     \
        goto err;                                           \
    }                                                       \
} while(0)

#define if_error_return(statement, status)    do {        \
    if (!(statement)) {                                     \
        return status;                                      \
    }                                                       \
} while(0)

#define setv_restrict(v, min, max) do {	\
            if ((v) < (min)) {                  \
                (v) = (min);                    \
            } else if ((v) > (max)) {           \
                (v) = (max);                    \
            }                                   \
        } while(0)     
        
#define setv_roll(v, min, max) do {	\
            if ((int)(v) < (min)) {                  \
                (v) = (max);                    \
            } else if ((v) > (max)) {           \
                (v) = (min);                    \
            }                                   \
        } while(0)                          
        
#define setu16_roll(v, min, max) do {	\
            if ((int16_t)(v) < (min)) {                  \
                (v) = (max);                    \
            } else if ((v) > (max)) {           \
                (v) = (min);                    \
            }                                   \
        } while(0)       

#define zmalloc(ptr, size) do {      \
        ptr = malloc(size);   \
        i_assert(ptr);              \
        if (ptr) {                  \
            memset(ptr, 0, size);    \
        }                           \
    } while(0)

void assert_failed(uint8_t *file, int line);

#define i_assert(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

#define TICK2MS(ticks) ((ticks) * (1000 /HZ))
#define MS2TICK(ms) ((ms) * FREQ/1000)

#define queue_init(queue) ((queue)->front = (queue)->rear = 0)
#define queue_is_empty(queue) ((queue)->front == (queue)->rear)
#define queue_is_full(queue, size) (((queue)->rear+1) % size == (queue)->front)

#endif
