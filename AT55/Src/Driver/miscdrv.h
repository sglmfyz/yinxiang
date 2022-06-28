#ifndef __MISCDRV_H__
#define __MISCDRV_H__

int miscdrv_init();
int lockio_open(int id);
int lockio_close(int id);

int flash_page_modify_value(uint32_t address, uint8_t *byte, uint16_t len);
int flash_page_finish();


#endif
