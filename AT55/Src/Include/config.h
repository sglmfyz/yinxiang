#ifndef __CONFIG_H__
#define __CONFIG_H__


/*---------- Heap config ------------*/

#define HEAP_ALIGNMENT 4
#define HEAP_ALIGN_MASK 0x3
#define HEAP_SIZE   (6 * 1024)


#define HZ 1000
#define FREQ 1000

//DEBUG
#define CONFIG_DBG 0



/*---------- SW Uart config ------------*/

#define SW_UART_RECV_BUF_SIZE 256 //默认的sw uart buf长度

#define SHELL_BUF_LEN 80
#define SHELL_PRINT_BUF_LEN 100
#define SHELL_CMD_MAX_LEN 10
#define SHELL_MAX_CMD_NUM 10
#define SHELL_MAX_ARGV 8
#define TASK_MAX_NUM 32
#define LOG_LINE_MAX_LEN 120

#define DEVKIT 0


#if DEVKIT == 1
#define TH_SENSOR 0
#define AT24_EEPROM 0
#define U485_LOCK1 0
#define U485_LOCK2 0
#else
#define TH_SENSOR 1
#define AT24_EEPROM 1
#define U485_LOCK1 1
#define U485_LOCK2 1

#endif

#define HAL_UART_MODULE_ENABLED 1
#define YY_SENSOR 0
#define INDIGO_SENSOR 0
#define U485_SENSOR 0
#define NET_PASSTHROUGH 0                                                                                                                                                                                                                                                                                                                                                                                                                  




#endif
