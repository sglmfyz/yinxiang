#ifndef __SHELL_H__
#define __SHELL_H__

#include "common.h"
#include "uart.h"

typedef struct Shell Shell_T;

/**
  * Shell回调函数类型
  * @param  this: this指针

  * @param  argc: 参数个数，不包含命令本身
  * @param  argv: 参数，不包含命令本身
*/

typedef int (*Shell_Callback_T)(Shell_T *this, void *caller, int argc, char *argv[]);

enum {
    SHELL_RT_OK,
    SHELL_RT_HELP,
};

struct Shell {
    /**
      * @brief  Shell注册函数
      * @param  this: this指针

      * @param  cb:  回调函数，参考Shell_Callback_T
      * @param  help: 该命令的帮助
      * @retval - 0：成功；
                - 其他值：失败原因；
      */
    int (*Register)(Shell_T *this, char *cmd, void *caller, Shell_Callback_T cb, char *help);
    /**
      * @brief  Shell打印函数，常用于命令回调处理
      * @param  fmt: 格式
      * @param  ...: 参数
    */
    void (*Print)(Shell_T *this, const char * fmt, ...);
    /**
      * @brief  Shell打印函数（带回车），常用于命令回调处理
      * @param  fmt: 格式
      * @param  ...: 参数
    */
    void (*Println)(Shell_T *this, const char * fmt, ...);


    void (*Set_Uart)(Shell_T *this, Uart_T *uart);
};
 
/**
  * @brief  Shell工厂函数
  * @param  uart: uart指针

  * @retval Shell指针
  */ 
Shell_T *Create_Shell(Uart_T *uart);

extern Shell_T *Main_Shell;

#endif
