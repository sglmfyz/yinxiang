#include "net.h"
#include "log.h"
#include "common.h"
#include "util.h"
#include "heap.h"
#include "etimer.h"
#include "process.h"

#if NET_TYPE == NET_GPRS

#define AT_TIMEOUT 10500
#define AT_MAX_RETRIES 5
#define ATTACH_MAX_RETRIES 12

#define AT_INIT_NUM (sizeof(GSM_RegCon_Cmd)/sizeof(GSM_RegCon_Cmd[0]))
#define GPRS_SENDER_TIMEOUT 15000
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

AT_Cmd_t GSM_RegCon_Cmd[] = {
    {"AT\n",      "OK"},
    {"AT+CGMR\n",      "OK"},
    {"ATE1\n", "OK"}, 
    {"AT+CPIN?\n","OK"},
    {"AT+CSQ\n",  "OK"},
    {"AT+CCID\n", "8986"},

    {"AT+COPS=4,0,\"CHINA MOBILE\"\n", "OK"},
    {"AT+CREG?\n","OK"},
    
    {"AT+CIPCLOSE=1\n","CLOSE OK|ERROR"},
    {"AT+CIPSHUT\n","SHUT OK"},
    
    {"AT+CSTT=\"CMIOT\"\n", "OK"},
    {"AT+CGATT?\n", "+CGATT: 1"},
    
    /*{"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\n", "OK"},
    {"AT+SAPBR=3,1,\"APN\",\"\"\n", "OK"},
    {"AT+SAPBR=1,1\n", "OK"},
    {"AT+SAPBR=2,1\n", "OK"},
    {"AT+CLBS=1,1\n", "+CLBS: "},*/
    //{"AT+SAPBR=0,1\n", "OK"},  //基站定位
    
    {"AT+CSQ\n",  "OK"},
    {"AT+CIICR\n", "OK"},
    {"AT+CIFSR\n", "1|2"},
    
    {"AT+CIPHEAD=1\n","OK"},
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
    uint8_t lsb[LSB_LEN];
} gprs_info_t;

PROCESS(_pwr_reg, "grps_reg");
PROCESS(_pkt_sender, "gprs_sender");

static void _hw_pwr(gprs_info_t *info, int on)
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

static void _handle_comm(gprs_info_t *info, void *data, int size)
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
        } else if (strncmp(p, "+IPD,", 5) == 0) {
            qh = p + 5;
            q = qh;
            while (*q >= '0' && *q <= '9') {
                q++;
            }
            p = q;
            if (*q == ':' && q != qh) {
                /*RECV a pkt, need to check the len*/
                *q = '\0';
                len = atoi(qh);
                if (len <= size - (q -p)) {
                    if (info->data_cb) {
                        info->data_cb(q+1, len);
                    }
                    p = q + 1 + len;
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

static void _gprs_msg_rx(Uart_T *uart, void *param, uint8_t *pkt, uint16_t pkt_size)
{
    //char buf[100];
    char *buf;
    char *p;
    gprs_info_t *info;
    Net_T *this = (Net_T *)param;

    info = container_of(this, gprs_info_t, public);
    buf = (char *)pkt;
    if (pkt_size > 0) {
        buf[pkt_size] = '\0';
        syslog_debug(buf);
    }
    
    if (info->send_state < AT_INIT_NUM) {
        p = keys_search(buf, GSM_RegCon_Cmd[info->reg_state].replyStr, '|', pkt_size);
        if (p) {
            if (strcmp(GSM_RegCon_Cmd[info->reg_state].replyStr, "8986") == 0) {
                memcpy(info->iccid, p, ICCID_LEN);
            } else if (strcmp(GSM_RegCon_Cmd[info->reg_state].replyStr, "+CLBS: ") == 0) {
                char *ps, *pe;
                int i;

                p += strlen("+CLBS: ");
                ps = NULL;
                pe = NULL;

                for (i=0; i<3; i++) {
                    while (*p != ',' && *p != 0) {
                        p++;
                    }
                    if (*p == 0) {
                        break;
                    } else if (i == 0) {
                        ps = p+1;
                    } else if (i == 2) {
                        pe = p;
                    }
                    p++;
                }
                syslog_debug("ps=%p, pe=%p\n", ps, pe);
                if (ps != NULL && pe != NULL) {
                    if (pe - ps >= LSB_LEN-1) {
                        memcpy(info->lsb, ps, LSB_LEN-1);
                        info->lsb[LSB_LEN-1] = '\0';
                    } else {
                        memcpy(info->lsb, ps, pe - ps);
                        info->lsb[pe-ps+1] = '\0';
                    }
                }
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
        if (keys_search((char *)pkt, "CONNECT OK", '|', pkt_size)) {
            syslog_debug("Connect OK!\n");
            info->send_state = SENDER_IDLE;
            info->reg_state++;
            process_post(info->net_worker, info->public.net_state_event, (process_data_t)info->send_state);
        }
    }
    else {
        _handle_comm(info, pkt, pkt_size);
    }
}

/*void _raw_mode(int on)
{
    gprs_info.raw_mode = on;
}*/
int _send_pkt(Net_T *this, void *pkt, int size)
{
    gprs_info_t *info;
    
    info = container_of(this, gprs_info_t, public);
    
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

    gprs_info_t *info;
    
    info = container_of(this, gprs_info_t, public);

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
    static gprs_info_t *info;    
    static struct etimer etimer;
    char send_cmd_buf[30];
    
    PROCESS_BEGIN();
    
    if (ev == PROCESS_EVENT_INIT) {
        info = (gprs_info_t *)args;
    }
    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
        
        sprintf(send_cmd_buf, "AT+CIPSEND=%d\n", info->pkt_size);
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
    static gprs_info_t *info;
    static int i;
    static struct etimer etimer;
    static int max_retry;

    PROCESS_BEGIN();
    
    if (ev == PROCESS_EVENT_INIT) {
        info = (gprs_info_t *)args;
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
        /*_hw_pwr(info, 0);
        etimer_set(&etimer, 1600);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        _hw_pwr(info, 1);
        etimer_set(&etimer, 100);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer)); //模块关闭

        syslog_critical("GPRS PowerDown\n");
        
        etimer_set(&etimer, 1500);  
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer)); //关机有一点延时，再开机有800ms延时

        syslog_critical("GPRS Restart\n");
        _hw_pwr(info, 0);
        etimer_set(&etimer, 1800);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        
        _hw_pwr(info, 1);        
        etimer_set(&etimer, 1500);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));*/
        _hw_pwr(info, 0);
        etimer_set(&etimer, 1500);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        _hw_pwr(info, 1);
        etimer_set(&etimer, 1300);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        _hw_pwr(info, 0);
        etimer_set(&etimer, 1200);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        
        _hw_pwr(info, 1);        
        etimer_set(&etimer, 3000);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
        syslog_debug("GPRS Power UP\n");

        syslog_critical("Register network now\n");

        while (1) {
            if (info->reg_state < sizeof(GSM_RegCon_Cmd)/sizeof(GSM_RegCon_Cmd[0])
                && strcmp(GSM_RegCon_Cmd[info->reg_state].cmdStr, "AT+CGATT?\n") == 0) {
                max_retry = ATTACH_MAX_RETRIES;
            } else {
                max_retry = AT_MAX_RETRIES;
            }
        
            while (i < max_retry) {
                _raw_send(info->uart, GSM_RegCon_Cmd[info->reg_state].cmdStr, strlen(GSM_RegCon_Cmd[info->reg_state].cmdStr));
                
                etimer_set(&etimer, AT_TIMEOUT);
                PROCESS_WAIT_EVENT_UNTIL(ev == info->rx_event || etimer_expired(&etimer));
                if (ev == info->rx_event) {
                    i = 0;
                    break;
                } else {
                    syslog_debug("Retransmit cmd %s", GSM_RegCon_Cmd[info->reg_state].cmdStr);
                    i++;
                }
            }

            if (i>= max_retry) {
                syslog_error("AT CMD failed...\n");
                if (info->reg_state <= 3) {
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
    gprs_info_t *info;
    
    info = container_of(this, gprs_info_t, public);

    if (info->send_state < SENDER_REG_OK) {
        return SENDER_NOT_READY;
    } else {
        return info->send_state;
    }
}

static int _set_cmd_callback(Net_T *this, Net_Cmd_Callback_T cb)
{
    gprs_info_t *info;

    info = container_of(this, gprs_info_t, public);
    info->cmd_cb = cb;

    return 0;
}
static int _set_data_callback(Net_T *this, Net_Data_Callback_T cb)
{
    gprs_info_t *info;

    info = container_of(this, gprs_info_t, public);
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
    gprs_info_t *info;
    
    info = container_of(this, gprs_info_t, public);
    info->net_worker = process;
    
    return 0;
}


static int _connect(Net_T *this, uint8_t proto, char *server, uint16_t port)
{
    char proto_name[5];    
    char server_AT[SERVER_AT_SIZE];
    gprs_info_t *info;
    
    info = container_of(this, gprs_info_t, public);
    if (info->send_state != SENDER_REG_OK) {
        return -STATE_ERR;
    }
    
    if (proto == PROTO_UDP) {
        strcpy(proto_name, "UDP");
    } else if (proto == PROTO_TCP) {
        strcpy(proto_name, "TCP");
    } else {
        return -PARAM_ERR;
    }
    snprintf(server_AT, SERVER_AT_SIZE, 
        "AT+CIPSTART=\"%s\",\"%s\",%d\n", proto_name, server, port);
    _raw_send(info->uart, server_AT, strlen(server_AT));
    return 0;
}

static int _get_specified(Net_T *this, int type, uint8_t *data)
{
    gprs_info_t *info;
    
    info = container_of(this, gprs_info_t, public);

    if (type == NET_SPEC_ICCID) {
        if (strncmp((char *)info->iccid, "8986", 4) == 0) {
            memcpy(data, info->iccid, ICCID_LEN);
            return 0;
        } else {
            return 2;
        }
    } else if (type == NET_SPEC_LSB && info->lsb[0] != 0) {
        memcpy(data, info->lsb, LSB_LEN);
        return 0;
    }

    return 1;
}

Net_T *Create_GPRS(Uart_T *uart, GPIO_TypeDef *GPIO_PWR, uint16_t PWR_Pin)
{
    gprs_info_t *info;
    Net_T *public;

    zmalloc(info, sizeof(gprs_info_t));
    info->uart = uart;
    info->gpio_pwr = GPIO_PWR;
    info->pwr_pin = PWR_Pin;
    info->rx_event = process_alloc_event();
    public = &info->public;

    uart->Set_Rx_Callback(uart, _gprs_msg_rx, public);
    
    
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
