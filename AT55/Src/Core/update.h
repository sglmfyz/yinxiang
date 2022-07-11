#ifndef __UPDATE_H__
#define __UPDATE_H__

#include "common.h"

//Flash size 256K改到64k?32k
#define FLASH_SIZE 0x10000
#define FLASH_BASE_ADDR 0x08000000
#define FLASH_END_ADDR  (FLASH_BASE_ADDR + FLASH_SIZE - 1)
#if 1
//Reserve end 64 kbytes for bootloader use
#define FLASH_FOR_BOOTLOADER_SIZE 0x8000 //Reserve 32k for bootloader
#define FLASH_RESERVED_SIZE 0x8000 //Reserve 32k for bootloader
#define FLASH_FOR_VAR_SIZE sizeof(update_flash_info_t)
#define UFLASH_BASE_ADDR (FLASH_BASE_ADDR + FLASH_FOR_BOOTLOADER_SIZE + FLASH_RESERVED_SIZE)
#define UFLASH_VAR_ADDR  (FLASH_BASE_ADDR + FLASH_SIZE - FLASH_FOR_VAR_SIZE)
//SRAM size 128K
#define RAM_SIZE_FOR_BOOT 0x4000 // 16k for RAM
//#define RAM_TOTAL_SIZE 0x20000 // 128K
#define RAM_TOTAL_SIZE (96*1024) // 96K, 保持96K的sram，镜像最大可以为96-16=80K

#define SRAM_BASE_ADDR 0x20000000
#define SRAM_END_ADDR (SRAM_BASE_ADDR + RAM_TOTAL_SIZE)
#define USRAM_VAR_SIZE sizeof(update_sram_info_t)
#define USRAM_VAR_ADDR (SRAM_END_ADDR - USRAM_VAR_SIZE)
#define USRAM_BIN_START (SRAM_BASE_ADDR + RAM_SIZE_FOR_BOOT)
#define USRAM_BIN_MAX_SIZE (RAM_TOTAL_SIZE- RAM_SIZE_FOR_BOOT - USRAM_VAR_SIZE)
#endif
#define BOARD_INFO_SIZE (sizeof(board_info_t))
#define BOARD_INFO_ADDR (FLASH_BASE_ADDR + FLASH_FOR_BOOTLOADER_SIZE - BOARD_INFO_SIZE)

#define usram_info ((update_sram_info_t *)(USRAM_VAR_ADDR))
//uflash info is ro var, needs to update manually
#define FLASH_ERASED 0xFFFF
#define FLASH_WRITE 0xEEEE
#define FLASH_BOOTED 0x0
#define FTP_BASE_DIR "/htdocs/download/"

#define MAX_NETWORK_RETRY 5
#define MAX_SYS_RETRY 3

#define UPDATE_STRESS_TEST 0

#define usram_info ((update_sram_info_t *)(USRAM_VAR_ADDR))
#define BOARD_CMD_EEPROM_BIT 0


typedef struct {
#if UPDATE_STRESS_TEST == 1
     uint16_t succeed;
     uint16_t failed;
#endif
    __IO uint16_t update;
    __IO uint16_t failed_major_v;
    __IO uint16_t failed_minor_v;
    __IO uint16_t format; //格式，目前为1
} update_sram_info_t;


typedef struct {
    uint16_t state;
    uint16_t major_v; //Cuurent user image's major number
    uint16_t minor_v; //Cuurent user image's minor number
    uint16_t format;  //格式，目前为1，该字段需要放在最后，也就是最高地址
} update_flash_info_t; //Ensure this structrure to be 4 bytes aligned.

typedef struct {
    uint8_t product_id; //硬件板卡类型，可包含厂商和类型信息
    uint8_t hw_version;//硬件版本号
    uint8_t cmd;
    uint8_t reserved;
    uint16_t dev_id; //设备号
    uint16_t format; //格式，目前为1
} board_info_t;

#define HW_VER_2G   2
#define HW_VER_4G   3



extern update_flash_info_t flash_info_g;
extern board_info_t board_info_g;

#define uflash_info (&flash_info_g)
#define board_info (&board_info_g)


#endif
