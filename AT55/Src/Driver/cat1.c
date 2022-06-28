#include "net.h"
#include "log.h"
#include "common.h"
#include "util.h"
#include "heap.h"
#include "etimer.h"
#include "process.h"

#if NET_TYPE == NET_CAT1

#define AT_TIMEOUT 10500
#define AT_MAX_RETRIES 5
#define ATTACH_MAX_RETRIES 12

#define AT_INIT_NUM (sizeof(CAT1_RegCon_Cmd)/sizeof(CAT1_RegCon_Cmd[0]))
#define CAT1_SENDER_TIMEOUT 15000
#define SERVER_AT_SIZE 100


enum {
    SENDER_CMD_SENDED = SENDER_IDLE+1,//发数据包时，首先发送命令
    SENDER_CMD_RESP,//命令响应回来
    SENDER_PKT_SENDED,
    SENDER_PKT_SENDOK,
};

typedef struct {
    char*   cmdStr;         // 指令字串
    char*   replyStr;       // 响应字串
} AT_Cmd_t;

AT_Cmd_t CAT1_RegCon_Cmd[] = {
    {"AT+CPIN?\r", "OK"},
    {"AT+QCCID\r", "OK"},

    {"AT+CREG?\r", "OK"},//查询是否注册CS业务
    {"AT+CGREG=2\r", "OK"}, //设置返回基站定位信息
    {"AT+CGREG?\r", "OK"}, //查询是否注册PS业务
    {"AT+NETCLOSE\r", "OK|ERROR"},


    {"AT+CGDCONT=1,\"IP\",\"CMIOT\"\r", "OK"},
    {"AT+NETOPEN\r", "+NETOPEN: 0"},
    {"AT+CIPOPEN=1,\"UDP\",,,5000\r", "+CIPOPEN: 1,0"},
};

typedef struct {
    Net_T public;
    int16_t reg_state;
    int16_t send_state;
    uint16_t pkt_size;
    int16_t raw_mode;

    uint16_t pwr_pin;    
    GPIO_TypeDef *gpio_pwr;

    Uart_T *uart;
    void *pkt_buf;
    Net_Data_Callback_T data_cb;
    Net_Cmd_Callback_T cmd_cb;
    process_event_t rx_event;
    struct process *net_worker;
    uint8_t iccid[ICCID_LEN];
    uint8_t server_addr[SERVER_ADDR_LEN];
    uint16_t server_port;
} cat1_info_t;

PROCESS(_pwr_reg, "cat1_reg");
PROCESS(_pkt_sender, "cat1_sender");

static void _hw_pwr(cat1_info_t *info, int on)
{
    if (on) {
        HAL_GPIO_WritePin(info->gpio_pwr, info->pwr_pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(info->gpio_pwr, info->pwr_pin, GPIO_PIN_SET);
    }
}
static inline void _raw_send(Uart_T *uart, void *data, int size)
{
    if (size == 0) {
        size = strlen((char *)data);
    }
    uart->Write_Bytes(uart, data, size);
}

static void _handle_comm(cat1_info_t *info, void *data, int size)
{
    char *p;
    char *q, *qh;
    int len;

    //*((char *)data+size+1) = '\0';
    //p = strtok_r((char *)data, "\n", &saved);
    p = (char *)data;
    while (p < (char *)data + size) {
        //syslog_debug("p: %s $$\n", p);
        if (strncmp(p, "> ", 2) == 0) {
            if (info->send_state == SENDER_CMD_SENDED) {
                info->send_state++;
                process_post(&_pkt_sender, info->rx_event, NULL);
            }
        } else if (strncmp(p, "SEND", 4) == 0) {
#if 0        
            if (info->send_state == SENDER_PKT_SENDED) {
                info->send_state = SENDER_IDLE;
            }
            if (strncmp(p+5, "OK", 2) == 0) {
                /*SEND SUCCEED, */

            } else {
                /*SEND FAILED*/
            }
#endif            
        } else if (strncmp(p, "+IPD", 4) == 0) {
            qh = p + 4;
            q = qh;
            while (*q >= '0' && *q <= '9') {
                q++;
            }
            p = q;
            if (*q == '\r' && q != qh) {
                /*RECV a pkt, need to check the len*/
                *q = '\0';
                 q += 2;//跳过末尾的\r\n
                 
                len = atoi(qh);
                if (len <= size - (q -p)) {
                    if (info->data_cb) {
                        info->data_cb(q+1, len);
                    }
                    p = q + len;
                    continue;
                    /*pkt OK*/
                } else {
                    /*pkt len error*/
                }
            }
            
        } else {
            if (info->cmd_cb) {
                q = p;
                while (*p != '\n' && p < (char *)data + size) {
                    p++;
                }
                info->cmd_cb(q, p - q);
            }
        }
        while (*p != '\n' && p < (char *)data + size) {
            p++;
        }
        p++;
    }
}

static void _cat1_msg_rx(Uart_T *uart, void *param, uint8_t *pkt, uint16_t pkt_size)
{
    //char buf[100];
    char *buf;
    char *p;
    cat1_info_t *info;
    Net_T *this = (Net_T *)param;

    info = container_of(this, cat1_info_t, public);
    buf = (char *)pkt;
    if (pkt_size > 0) {
        buf[pkt_size] = '\0';
        syslog_debug("Got %d: %s$\n", strlen(buf), buf);
    }
    
    if (info->send_state < AT_INIT_NUM) {
        p = keys_search(buf, CAT1_RegCon_Cmd[info->reg_state].replyStr, '|', pkt_size);
        if (p) {
            if (strcmp(CAT1_RegCon_Cmd[info->reg_state].replyStr, "8986") == 0) {
                memcpy(info->iccid, p, ICCID_LEN);
            } 
            if (info->reg_state == info->send_state) {
                info->reg_state++;
            } else {
                return;
            }

            if (info->reg_state == AT_INIT_NUM) {
                info->send_state = SENDER_REG_OK;
                syslog_debug("Post reg OK!\n");
                process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
            } 
            process_post(&_pwr_reg, info->rx_event, NULL);
        }
    } else if (info->send_state == SENDER_REG_OK) {
        /*说明正在连接*/
        syslog_debug("Connect OK!\n");
        info->send_state = SENDER_IDLE;
        info->reg_state++;
        process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
    }
    else {
        _handle_comm(info, pkt, pkt_size);
    }
}

/*void _raw_mode(int on)
{
    cat1_info.raw_mode = on;
}*/
int _send_pkt(Net_T *this, void *pkt, int size)
{
    cat1_info_t *info;
    
    info = container_of(this, cat1_info_t, public);
    
    if (info->send_state == SENDER_IDLE) {
        info->send_state = SENDER_CMD_SENDED; //先占住命令通道
        info->pkt_buf = pkt;
        info->pkt_size = size;
        process_post_synch(&_pkt_sender, PROCESS_EVENT_POLL, info);
        return 0;
    } else {
        return -1;
    }
}
int _send_cmd(Net_T *this, char *cmd)
{

    cat1_info_t *info;
    
    info = container_of(this, cat1_info_t, public);

    if (info->send_state == SENDER_IDLE) {
        _raw_send(info->uart, cmd, strlen(cmd));
        _raw_send(info->uart, "\n", 1);
        info->send_state = SENDER_IDLE;

        return 0;
    } else {
        return -1;
    }
}

PROCESS_THREAD(_pkt_sender, ev, args)
{
    static cat1_info_t *info;    
    static struct etimer etimer;
    char send_cmd_buf[100];
    
    PROCESS_BEGIN();
    
    if (ev == PROCESS_EVENT_INIT) {
        info = (cat1_info_t *)args;
    }
    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
        
        sprintf(send_cmd_buf, "AT+CIPSEND=1,%d,\"%s\",%d\r", info->pkt_size, info->server_addr, info->server_port);//发送可变长度
        
        _raw_send(info->uart, send_cmd_buf, 0);
        info->send_state = SENDER_CMD_SENDED;

        etimer_set(&etimer, 3000);
        PROCESS_WAIT_EVENT_UNTIL(ev == info->rx_event || etimer_expired(&etimer));

        _raw_send(info->uart, info->pkt_buf, info->pkt_size);
        info->send_state = SENDER_IDLE;
        
    }
    PROCESS_END();
}

PROCESS_THREAD(_pwr_reg, ev, args)
{
    static cat1_info_t *info;
    static int i;
    static struct etimer etimer;
    static int max_retry;

    PROCESS_BEGIN();
    
    if (ev == PROCESS_EVENT_INIT) {
        info = (cat1_info_t *)args;
    }

    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_SERVICE_REMOVED) {
            _hw_pwr(info, 0);
            etimer_set(&etimer, 3000);
            PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
            _hw_pwr(info, 1);
            syslog_debug("Do real powerdown\n");
            continue;
        } else if (ev != PROCESS_EVENT_POLL) {
            continue;
        }
        info->reg_state = 0;
        info->send_state = 0;
        i=0;
        syslog_critical("Reset network now\n");
        
        _hw_pwr(info, 0);
        etimer_set(&etimer, 1000);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        _hw_pwr(info, 1);
        etimer_set(&etimer, 5000);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        _hw_pwr(info, 0);
        etimer_set(&etimer, 1200);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        
        _hw_pwr(info, 1);        
        etimer_set(&etimer, 10000);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        syslog_debug("Cat1 Power UP\n");

        syslog_critical("Register network now\n");

        while (1) {
            max_retry = AT_MAX_RETRIES;
        
            while (i < max_retry) {
                _raw_send(info->uart, CAT1_RegCon_Cmd[info->reg_state].cmdStr, strlen(CAT1_RegCon_Cmd[info->reg_state].cmdStr));
                syslog_debug("Send At cmd: %s\n", CAT1_RegCon_Cmd[info->reg_state].cmdStr);
                etimer_set(&etimer, AT_TIMEOUT);
                PROCESS_WAIT_EVENT_UNTIL(ev == info->rx_event || etimer_expired(&etimer));
                if (ev == info->rx_event) {
                    i = 0;
                    break;
                } else {
                    syslog_debug("Retransmit cmd %s", CAT1_RegCon_Cmd[info->reg_state].cmdStr);
                    i++;
                }
            }

            if (i>= max_retry) {
                syslog_error("AT CMD failed...\n");
                if (info->reg_state <= 1) {
                    info->send_state = SENDER_AT_FAIL;
                } else {
                    info->send_state = SENDER_NOT_READY;
                }
                process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
                break;
            } else  if (info->send_state < info->reg_state) {
                info->send_state = info->reg_state;
            }

            if (info->send_state >= SENDER_REG_OK) {
                /*注册结束，线程进入睡眠*/
                break;
            }
        }
    }
    
    PROCESS_END();
}


static int _get_state(Net_T *this)
{
    cat1_info_t *info;
    
    info = container_of(this, cat1_info_t, public);

    if (info->send_state < SENDER_REG_OK) {
        return SENDER_NOT_READY;
    } else {
        return info->send_state;
    }
}

static int _set_cmd_callback(Net_T *this, Net_Cmd_Callback_T cb)
{
    cat1_info_t *info;

    info = container_of(this, cat1_info_t, public);
    info->cmd_cb = cb;

    return 0;
}
static int _set_data_callback(Net_T *this, Net_Data_Callback_T cb)
{
    cat1_info_t *info;

    info = container_of(this, cat1_info_t, public);
    info->data_cb = cb;

    return 0;
}
static int _reset(Net_T *this)
{
    process_poll(&_pwr_reg);
    return 0;
}

static int _poweroff(Net_T *this)
{
    process_post(&_pwr_reg, PROCESS_EVENT_SERVICE_REMOVED, NULL);

    return 0;
}

static int _set_networker(Net_T *this, struct process *process)
{
    cat1_info_t *info;
    
    info = container_of(this, cat1_info_t, public);
    info->net_worker = process;
    
    return 0;
}


static int _connect(Net_T *this, uint8_t proto, char *server, uint16_t port)
{
    cat1_info_t *info;
    
    info = container_of(this, cat1_info_t, public);
    if (info->send_state != SENDER_REG_OK) {
        return -STATE_ERR;
    }

    
    i_assert(proto == PROTO_UDP || proto == PROTO_TCP);
    i_assert (strlen(server) <= sizeof(info->server_addr));
    memcpy(info->server_addr, server, strlen(server));
    info->server_port = port;
    
    return 0;
}

static int _get_specified(Net_T *this, int type, uint8_t *data)
{
    cat1_info_t *info;
    
    info = container_of(this, cat1_info_t, public);

    if (type == NET_SPEC_ICCID) {
        if (strncmp((char *)info->iccid, "8986", 4) == 0) {
            memcpy(data, info->iccid, ICCID_LEN);
            return 0;
        } else {
            return 2;
        }
    } else if (type == NET_SPEC_LSB) {
        return 1;
    }

    return 1;
}

Net_T *Create_CAT1(Uart_T *uart, GPIO_TypeDef *GPIO_PWR, uint16_t PWR_Pin)
{
    cat1_info_t *info;
    Net_T *public;

    zmalloc(info, sizeof(cat1_info_t));
    info->uart = uart;
    info->gpio_pwr = GPIO_PWR;
    info->pwr_pin = PWR_Pin;
    info->rx_event = process_alloc_event();
    public = &info->public;

    uart->Set_Rx_Callback(uart, _cat1_msg_rx, public);
    
    
    public->Set_Cmd_Callback = _set_cmd_callback;
    public->Set_Data_Callback = _set_data_callback;
    public->Reset = _reset;
    public->Power_Off = _poweroff;
    public->Get_State = _get_state;
    public->Send_Cmd = _send_cmd;
    public->Send_Pkt = _send_pkt;
    public->Connect = _connect;
    public->Set_Networker = _set_networker;
    public->Get_Specified = _get_specified;

    process_start(&_pwr_reg, (const char *)info);
    process_start(&_pkt_sender, (const char *)info);
    
    public->net_state_event = process_alloc_event();
    
    return public;
}

#endif
