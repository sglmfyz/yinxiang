#include <stdarg.h>
#include "shell.h"
#include "config.h"
#include "heap.h"
#include "log.h"

#define PS "# "

typedef struct {
    char cmd[SHELL_MAX_CMD_NUM][SHELL_CMD_MAX_LEN + 1];
    void *caller[SHELL_MAX_CMD_NUM];
    Shell_Callback_T fn[SHELL_MAX_CMD_NUM];
    char *help[SHELL_MAX_CMD_NUM];
    uint8_t num;
} cmd_info_t;

typedef  struct {
    char *argv[SHELL_MAX_ARGV];
    int argc;
} process_buf_t;

typedef struct {
    Shell_T public;
    Uart_T *uart;
    uint8_t len;
    char buf[SHELL_BUF_LEN + 1];
    char print_buf[SHELL_PRINT_BUF_LEN + 1];
    cmd_info_t cmd_info;
    process_buf_t process_buf;
} shell_info_t;

static int _register ( Shell_T *shell, char *cmd, void *caller, Shell_Callback_T fn, char *help)
{
    uint8_t cmd_len;
    shell_info_t *info;

    info = container_of(shell, shell_info_t, public);
    cmd_info_t *cmd_info = &info->cmd_info;

    if ( cmd_info->num < SHELL_MAX_CMD_NUM ) {
        cmd_len = ( strlen ( cmd ) < SHELL_CMD_MAX_LEN ) ? strlen ( cmd ) : ( SHELL_CMD_MAX_LEN );
        memcpy ( cmd_info->cmd[cmd_info->num], cmd, cmd_len );
        cmd_info->cmd[cmd_info->num][cmd_len] = '\0';
        cmd_info->fn[cmd_info->num] = fn;
        cmd_info->help[cmd_info->num] = help;        
        cmd_info->caller[cmd_info->num] = caller;
        cmd_info->num++;
        return 0;
    } else {
        return -NO_SPACE_ERR;
    }
}

static void _shell_process (shell_info_t *info, char *buf)
{
    int i, j;
    uint8_t len;
    cmd_info_t *cmd_info = &info->cmd_info;
    process_buf_t *process_buf = &info->process_buf;
    int status;

    len = strlen ( buf );

    for ( j = 0; j < cmd_info->num; j++ ) {
        if ( memcmp ( cmd_info->cmd[j], buf, strlen ( cmd_info->cmd[j] ) ) == 0 ) {
            process_buf->argc = 0;
            for ( i = 0; i < len - 1; i++ ) {
                if ( buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\r' ) {
                    buf[i] = '\0';
                    if ( i > 0 && ( buf[i + 1] == ' ' || buf[i + 1] == '\n' ||
                                    buf[i + 1] == '\r' ) ) {
                        continue;
                    }
                    process_buf->argv[process_buf->argc] = &buf[i + 1];
                    process_buf->argc++;
                    if ( process_buf->argc >= SHELL_MAX_ARGV ) {
                        break;
                    }
                }
            }
            status = cmd_info->fn[j] ( &info->public, cmd_info->caller, process_buf->argc, process_buf->argv );
            if (status == SHELL_RT_HELP) {
                info->public.Print(&info->public, cmd_info->help[j]);
            }
        }
    }
}

static void _print(Shell_T *shell, const char * fmt, ...)
{
    shell_info_t *info;
    char *p;
    int n;
    va_list ap;
    
    info = container_of(shell, shell_info_t, public);
    p = (char *)info->print_buf;
    va_start(ap, fmt);
    n = vsnprintf(p, SHELL_PRINT_BUF_LEN, fmt, ap);

    info->uart->Write_Bytes(info->uart, (uint8_t *)p, n);

    va_end(ap);

}
static void _println(Shell_T *shell, const char * fmt, ...)
{
    shell_info_t *info;
    char *p;
    int n;
    va_list ap;
    
    info = container_of(shell, shell_info_t, public);
    p = (char *)info->print_buf;
    va_start(ap, fmt);
    n = vsnprintf(p, SHELL_PRINT_BUF_LEN, fmt, ap);
    p[n++] = '\n';

    info->uart->Write_Bytes(info->uart, (uint8_t *)p, n);

    va_end(ap);

}

static void _set_uart(Shell_T *shell, Uart_T *uart)
{
    shell_info_t *info;

    info = container_of(shell, shell_info_t, public);

    info->uart = uart;
}

static void shell_process(Uart_T *uart, void *this, uint8_t *pkt, uint16_t size)
{
    uint8_t value;
    int i;
    shell_info_t *info;

    info = container_of(this, shell_info_t, public);

    for (i=0; i<size; i++) {
        value = *((uint8_t *)pkt + i);

        if ( info->len >= SHELL_BUF_LEN ) {
            info->len = 0;
        }

        if ( value == 127 ) { /*DEL*/
            if ( info->len > 0 ) {
                info->len--;
                _print( &info->public, "%c", value );
            }
        } else if ( value != 27 ) {
            info->buf[info->len++] = value;
            if ( value != '\n' && value != '\r' ) {
                _print( &info->public, "%c", value );
            }
        } else {
            /*ESC key*/
            info->len = 0;
        }

        if ( value == '\n' || value == '\r' ) {
            info->buf[info->len - 1] = '\0';
            _print( &info->public, "\n" );
            _shell_process ( info, info->buf );
            _print( &info->public, "%s ", PS );
            info->len = 0;
        }
    }
}
static int _do_help(Shell_T *shell, void *caller, int argc, char *argv[])
{
    int i;
    shell_info_t *info;
    cmd_info_t *cmd_info;

    info = container_of(shell, shell_info_t, public);
    cmd_info = &info->cmd_info;
    
    if (argc >= 1) {
        for(i=0; i<cmd_info->num; i++) {
            if (strcmp(cmd_info->cmd[i], argv[0]) == 0) {
                shell->Println(shell, "%s:", cmd_info->cmd[i]);
                shell->Println(shell, "%s", cmd_info->help[i]);
                break;
            } 
        }
        if (i >= cmd_info->num) {
            shell->Println(shell, "Not found cmd \"%s\", use help to list all commands", argv[0]);
        }
    } else {
        shell->Println(shell, "Help list:");
        shell->Println(shell, "--------------");
        for(i=0; i<cmd_info->num; i++) {
            shell->Println(shell, "%s:", cmd_info->cmd[i]);
            shell->Println(shell, "%s", cmd_info->help[i]);
        }
    }
    return SHELL_RT_OK;
}

Shell_T *Create_Shell(Uart_T *uart)
{
    shell_info_t *info;
    Shell_T *public;

    zmalloc(info, sizeof(shell_info_t));

    info->uart = uart;

    public = &info->public;
    public->Register = _register;
    public->Print = _print;
    public->Println = _println;
    public->Set_Uart = _set_uart;

    if (info->uart) {
        uart->Set_Rx_Callback(uart, shell_process, public);
    }
    
    _print( &info->public, "%s", PS);
    _register(public, "help", NULL, _do_help, "  help\n  help <cmd>\n");
    return public;
}


