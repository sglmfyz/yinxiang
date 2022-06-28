#include "net.h"
#include "log.h"
#include "common.h"
#include "util.h"
#include "heap.h"
#include "etimer.h"
#include "process.h"
#include "shell.h"

#if NET_TYPE == NET_WIFI

#define AT_TIMEOUT 3000
#define AT_MAX_RETRIES 2
#define AT_INIT_NUM sizeof(wifi_cmd)/sizeof(wifi_cmd[0])
typedef struct {
    Net_T public;
    Uart_T *uart;
    GPIO_TypeDef *port_reset, *port_reload, *port_ready;
    uint16_t pin_reset, pin_reload, pin_ready;
    
    Net_Data_Callback_T data_cb;
    Net_Cmd_Callback_T cmd_cb;
    process_event_t rx_event;
    struct process *net_worker;

    uint8_t reg_state, send_state;
} wifi_info_t;

typedef struct {
    char*   cmdStr;         // 指令字串
    char*   replyStr;       // 响应字串
} AT_Cmd_t;

AT_Cmd_t wifi_cmd[] = {
    {"AT+E=on\r", "+ok"},
    {"AT+WMODE=STA\r", "+ok"},
    {"AT+E=on\r", "+ok"},
    {"AT+WSSSID=aqc2.4G\r", "+ok"},
    {"AT+WSKEY\r", "+ok"},
    {"AT+WSKEY=WPAPSK,AES,aqc@1234\r", "+ok"},

};

PROCESS(_pwr_reg, "");
//PROCESS(_pkt_sender, "");
PROCESS(_wifi_reader, "");


static inline void _raw_send(Uart_T *uart, void *data, int size)
{
    if (size == 0) {
        size = strlen((char *)data);
    }
    uart->Write_Bytes(uart, data, size);
}

static void _wifi_msg_rx(wifi_info_t *info, void *data, int size)
{
    //char buf[100];
    char *buf;

    buf = (char *)data;
    if (size > 0) {
        buf[size] = '\0';
        syslog_debug(buf);
    }
    
    if (info->send_state < AT_INIT_NUM) {
        if (keys_search((char *)data, wifi_cmd[info->reg_state].replyStr, '|', size)) {
            if (info->reg_state == info->send_state) {
                info->reg_state++;
            } else {
                return;
            }
            syslog_debug("reg state=%d, end=%d\n", info->reg_state, AT_INIT_NUM);

            if (info->reg_state == AT_INIT_NUM) {
                info->send_state = SENDER_REG_OK;
                syslog_debug("Post reg OK!\n");
                if (info->net_worker) {
                    process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
                }
            } 
            process_post(&_pwr_reg, info->rx_event, NULL);
        }
    } else if (info->send_state == SENDER_REG_OK) {
        /*说明正在连接*/
        if (keys_search((char *)data, "CONNECT OK", '|', size)) {
            info->send_state = SENDER_IDLE;
            process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
        }
    }
    else {
        //_handle_comm(info, data, size);
    }
}



PROCESS_THREAD(_pwr_reg, ev, args)
{
    static wifi_info_t *info;
    static uint8_t i;
    static struct etimer etimer;

    PROCESS_BEGIN();
    
    if (ev == PROCESS_EVENT_INIT) {
        info = (wifi_info_t *)args;
    }

    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
        info->send_state = 0;
        info->reg_state = 0;
        i=0;

        syslog_critical("Reset wifi now\n");

        syslog_debug("Ready:%d\n", 
        HAL_GPIO_ReadPin(info->port_ready, info->pin_ready));
        
        HAL_GPIO_WritePin(info->port_reset, info->pin_reset, GPIO_PIN_RESET);
        etimer_set(&etimer, 100);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        
        HAL_GPIO_WritePin(info->port_reset, info->pin_reset, GPIO_PIN_SET);

        syslog_critical("Wifi PowerUp\n");
        etimer_set(&etimer, 100);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        syslog_debug("Ready2:%d\n", 
            HAL_GPIO_ReadPin(info->port_ready, info->pin_ready));

        _raw_send(info->uart, "+++", 3);
        etimer_set(&etimer, 500);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        _raw_send(info->uart, "a", 1);

        //上电成功
        while (1) {
            while (i < AT_MAX_RETRIES) {
                _raw_send(info->uart, wifi_cmd[info->reg_state].cmdStr, strlen(wifi_cmd[info->reg_state].cmdStr));
                
                etimer_set(&etimer, AT_TIMEOUT);
                PROCESS_WAIT_EVENT_UNTIL(ev == info->rx_event || etimer_expired(&etimer));
                if (ev == info->rx_event) {
                    i = 0;
                    break;
                } else {
                    syslog_debug("Retransmit cmd %d, %s", info->reg_state, wifi_cmd[info->reg_state].cmdStr);
                    i++;
                }
            }

            if (i>= AT_MAX_RETRIES) {
                syslog_error("AT CMD failed...\n");
                info->send_state = SENDER_NOT_READY;
                //process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
                break;
            } else if (info->send_state < info->reg_state) {
                info->send_state = info->reg_state;
            }

            if (info->reg_state >= SENDER_REG_OK) {
                /*注册结束，线程进入睡眠*/
                break;
            }
        }
    }
    PROCESS_END();
}

PROCESS_THREAD(_wifi_reader, ev, args)
{
    static wifi_info_t *info;
    rx_pkt_t *pkt;
    
    PROCESS_BEGIN();
    
    if (ev == PROCESS_EVENT_INIT) {
        info = (wifi_info_t *)args;
    }
    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(ev == uart_rx_event);
        pkt = (rx_pkt_t *)args;
        _wifi_msg_rx(info, pkt->pkt, pkt->size);
    } 

    PROCESS_END();
}

static int _set_cmd_callback(Net_T *this, Net_Cmd_Callback_T cb)
{
    wifi_info_t *info;

    info = container_of(this, wifi_info_t, public);
    info->cmd_cb = cb;

    return 0;
}
static int _set_data_callback(Net_T *this, Net_Data_Callback_T cb)
{
    wifi_info_t *info;

    info = container_of(this, wifi_info_t, public);
    info->data_cb = cb;

    return 0;
}
static int _set_networker(Net_T *this, struct process *process)
{
    wifi_info_t *info;
    
    info = container_of(this, wifi_info_t, public);
    info->net_worker = process;
    
    return 0;
}

static int _reset(Net_T *this)
{
    process_poll(&_pwr_reg);
    return 0;
}
static int _wifi_uart(Shell_T *this, void *caller, int argc, char *argv[])
{
    int i;
    wifi_info_t *info;

    info = (wifi_info_t *)caller;
    for (i=0; i<argc; i++) {
        _raw_send(info->uart, argv[i], strlen(argv[i]));
        if (i != argc - 1) {
            _raw_send(info->uart, " ", 1);
        } else {
            _raw_send(info->uart, "\r", 1);
        }
    }
    return SHELL_RT_OK;
}



Net_T *Create_Wifi(Uart_T *uart, GPIO_TypeDef *GPIO_Resetn, uint16_t Resetn_Pin, 
    GPIO_TypeDef *GPIO_Reloadn, uint16_t Reloadn_Pin, GPIO_TypeDef *GPIO_Readyn, uint16_t Readyn_Pin)
{
    wifi_info_t *info;
    Net_T *public;

    zmalloc(info, sizeof(wifi_info_t));

    info->port_reset = GPIO_Resetn;
    info->pin_reset = Resetn_Pin;

    info->port_reload = GPIO_Reloadn;
    info->pin_reload = Reloadn_Pin;

    info->port_ready = GPIO_Readyn;
    info->pin_ready = Readyn_Pin;
    info->uart = uart;

    syslog_debug("Ready Init:%d\n", 
            HAL_GPIO_ReadPin(info->port_ready, info->pin_ready));

    HAL_GPIO_WritePin(info->port_reset, info->pin_reset, GPIO_PIN_SET);
    HAL_GPIO_WritePin(info->port_reload, info->pin_reload, GPIO_PIN_SET);
    
    uart->Set_Rx_Process(uart, &_wifi_reader);

    public = &info->public;
    public->Set_Cmd_Callback = _set_cmd_callback;
    public->Set_Data_Callback = _set_data_callback;
    public->Set_Networker = _set_networker;
    public->Reset = _reset;
    //public->Get_State = _get_state;
    /*public->Send_Cmd = _send_cmd;
    public->Send_Pkt = _send_pkt;
    public->Connect = _connect;*/

    process_start(&_pwr_reg, (const char *)info);
    //process_start(&_pkt_sender, (const char *)info);
    process_start(&_wifi_reader, (const char *)info);
    
    public->net_state_event = process_alloc_event();

    //Main_Shell->Register(Main_Shell, "wifi", info, _wifi_uart, "wifi cmd");
    return public;
}
    
#endif
