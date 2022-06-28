#include "common.h"
#include "miscdrv.h"
#include "gpio.h"
#include "etimer.h"
#include "process.h"
#include "heap.h"
#include "update.h"
#include "log.h"
#include "main.h"

static process_event_t lock_event;

#define PAGE_SIZE (2 * 1024)
static uint8_t *flash_page_buf;
static uint32_t ongoing_page;

PROCESS(_lock_thread, "");
PROCESS_THREAD(_lock_thread, ev, data)
{
    static struct etimer etime;
    static int lock_id;
    
    PROCESS_BEGIN();

    while (TRUE) {
        PROCESS_WAIT_EVENT_UNTIL(ev == lock_event);
        lock_id = (int)data;

        if (lock_id == 1) {
            GPIO_Write_Hi(LOCK3_CTL_GPIO_Port, LOCK3_CTL_Pin);
        } else if (lock_id == 2) {
            GPIO_Write_Hi(LOCK4_CTL_GPIO_Port, LOCK4_CTL_Pin);
        }
        
        etimer_set(&etime, 5000);
        PROCESS_WAIT_UNTIL(etimer_expired(&etime));

        if (lock_id == 1) {
            GPIO_Write_Low(LOCK3_CTL_GPIO_Port, LOCK3_CTL_Pin);
        } else if (lock_id == 2) {
            GPIO_Write_Low(LOCK4_CTL_GPIO_Port, LOCK4_CTL_Pin);
        }
    }

    PROCESS_END();
}


int miscdrv_init()
{
    lock_event = process_alloc_event();
    process_start(&_lock_thread, NULL);
    zmalloc(flash_page_buf, PAGE_SIZE);
    return 0;
}

static int _flash_page_read(int page)
{
    int i;
    uint32_t faddr = FLASH_BASE_ADDR + page * PAGE_SIZE;
    uint32_t *p = (uint32_t *)flash_page_buf;

    if (ongoing_page != 0 && ongoing_page != page) {
        return -1;
    }

    if (ongoing_page == 0) {
        ongoing_page = page;
        i = 0;
        
        while (i < PAGE_SIZE) {
            *p++ = *(__IO uint32_t*)(faddr+i);
            i += 4;
        }
    }
    return 0;
}

int flash_page_modify_value(uint32_t address, uint8_t *byte, uint16_t len)
{
    int status;
    int page = (address - FLASH_BASE_ADDR)/PAGE_SIZE;
    int offset = address - FLASH_BASE_ADDR - (page * PAGE_SIZE);

    if (offset + len >= PAGE_SIZE) {
        return PARAM_ERR;
    }
    
    status = _flash_page_read(page);
    if (status != 0) {
        return STATE_ERR;
    }    
    
    for (int i=0; i<len; i++) {
        flash_page_buf[offset+i] = byte[i];
    }
    
    return 0;
}
int flash_page_finish()
{
    int status;
    uint32_t addr;
    uint32_t *p;
    
    if (ongoing_page == 0) {
        return STATE_ERR;
    }

    addr = FLASH_BASE_ADDR + ongoing_page * PAGE_SIZE;
    flash_unlock();
    Delay_ms(80);
    flash_sector_erase(addr);
    //FLASH_ErasePage(addr);没有这玩意类型也没有，不知道能不能用sectorerase替换
    

    p = (uint32_t *)flash_page_buf;
    for (int i=0; i<PAGE_SIZE; i+=4) {
        status = flash_word_program(addr + i, *p++);
        if (status != FLASH_OPERATE_DONE) {
            syslog_error("write error, addr %08x, status %d \n", addr, status);
            break;
        }
    }
    
    flash_lock();

    if (status == FLASH_OPERATE_DONE) {
        return 0;
    } else {
        return OPERATOR_ERR;
    }
}





int lockio_open(int id)
{
    process_post(&_lock_thread, lock_event, (void *)id);
    return 0;
}


