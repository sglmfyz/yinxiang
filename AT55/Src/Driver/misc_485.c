#include "misc_485.h"
#include "process.h"
#include "etimer.h"
#include "hw.h"
#include "log.h"
#include "modbus_regs.h"
#include "heap.h"

#define MAX_MISC_BUFFER 32
#define MAX_485_IFS 2 // 0对应uart3，1对应uart6

static Uart_T *uart485[2] = {NULL, NULL};

enum {
    LOCK_485,
    LOCK_RELAY,
    MAX_DEVICE_TYPES
};

typedef struct {
    uint8_t *buf;
    uint16_t len;
} probe_msg_t;

typedef struct {
    Misc485_T public;
    uint8_t buf[MAX_MISC_BUFFER];
    uint16_t buf_num;
    uint32_t last_ms;
    misc485_cb misc_cb;
    Lock_Callback lock_cb;    
    
    Lock_485_T *lock_485[MAX_485_IFS];
    Relay_485_T *relay_485[MAX_485_IFS];
    int stage;    
    probe_msg_t probe_msg[MAX_DEVICE_TYPES];
    struct {
        Uart_T *uart;
        uint16_t mask;
    } probe_info[MAX_485_IFS];
} misc485_info_t;


enum {
    DETECT_START,
    DETECT_LOCK_FINISH,
    DETECT_FINISHED,
};

enum {
    LOCK485_MASK_BIT = 0,
    RELAY485_MASK_BIT,
};

static void _misc_cb(Uart_T *this, void *param, uint8_t *pkt, uint16_t pkt_size)
{
    misc485_info_t *misc485_info = (misc485_info_t *)param;
    int i, j;
    uint32_t v;

    if (misc485_info->buf_num > 0) {
        if (Millis() - misc485_info->last_ms >= 1000) {
            misc485_info->buf_num = 0;
        }
    }
    misc485_info->last_ms = Millis();
    
    int copy = min(pkt_size, MAX_MISC_BUFFER - misc485_info->buf_num);
    
    memcpy(misc485_info->buf + misc485_info->buf_num, pkt, copy);
    misc485_info->buf_num += copy;

    if (misc485_info->buf_num >= 12) {
        uint8_t *p = misc485_info->buf;
        v  = 0;
        if (p[0] == 0x2 && p[9] == 0x0d && p[10] == 0x0a && p[11] == 0x03) {
            for (i=1; i<9; i++) {
                if (p[i] >= '0' && p[i] <= '9') {
                    v = v | ((p[i] - '0') << (32 - 4*i));
                } else if (p[i] >= 'a' && p[i] <= 'f') {
                    v = v | ((p[i] - 'a' + 0xa) << (32 - 4*i));
                } else if (p[i] >= 'A' && p[i] <= 'F') {
                    v = v | ((p[i] - 'A' + 0xa) << (32 - 4*i));
                } else {
                    break;
                }
            }
            if (i >= 9) {
                for (j=0; j<MAX_485_IFS; j++) {
                    if (this == uart485[i]) {
                        break;
                    }
                }
                misc485_info->misc_cb(MISC485_TYPE_READER, j, &v);
            }
        }
        misc485_info->buf_num = 0;
    }
    
}

static void _misc_probe_rx(Uart_T *this, void *param, uint8_t *pkt, uint16_t pkt_size)
{
    int i;
    Lock_485_T *lock;
    Relay_485_T *relay;
    misc485_info_t *misc485_info = (misc485_info_t *)param;

    syslog_raw("Recv: ");
    dump_buffer(pkt, pkt_size);

    
    for (i=0; i<MAX_485_IFS; i++) {
        if (this == uart485[i]) {
            break;
        }
    }
    
    if (i>= MAX_485_IFS) {
        syslog_error("Wrong uart: %p\n", this);
        return;
    }
    
    lock = misc485_info->lock_485[i];
    relay = misc485_info->relay_485[i];
    if (lock->Is_Mounted(lock, pkt, pkt_size)) {
        bit_set(misc485_info->probe_info[i].mask, LOCK485_MASK_BIT);
    } else if (relay->Is_Mounted(relay, pkt, pkt_size)) {
        bit_set(misc485_info->probe_info[i].mask, RELAY485_MASK_BIT);
    }
}


static void _probe_misc485(misc485_info_t *info, int probe_type)
{
    int i, n;
    Uart_T *uart;
    n = 0;
    
    for (i=0; i<MAX_485_IFS; i++) {
        if (info->probe_info[i].mask == 0) {
            // 还没探测到
            n++;
            uart = info->probe_info[i].uart;
            uart->Write_Bytes(uart, info->probe_msg[probe_type].buf, info->probe_msg[probe_type].len);
            syslog_debug("Send uart %d, type:%d, len:%d\n", i, probe_type, info->probe_msg[probe_type].len);
            //dump_buffer(info->probe_msg[probe_type].buf, info->probe_msg[probe_type].len);
        }
    }
    if (n == 0) {
        // 已经全部探测到了，探测结束
        info->stage = DETECT_LOCK_FINISH;
    }
}


PROCESS(_misc485_detect, "misc485_detect");

PROCESS_THREAD(_misc485_detect, ev, args)
{
    static struct etimer etimer;
    static misc485_info_t *info;
    static int probe_type = 0;
    static int start;
    int hw_config;
    int i;
    Lock_485_T *lock;
    Relay_485_T *relay;

    PROCESS_BEGIN();

    if (ev == PROCESS_EVENT_INIT) {
        info = (misc485_info_t *)args;
    }

    start = Millis();

    while (1) {
        hw_config = get_input_reg(UREG3X_HW_CONFIG);
        if (info->stage == DETECT_START) {
            _probe_misc485(info, probe_type);
            if (Millis() - start >= 5000) {
                info->stage = DETECT_LOCK_FINISH;
                continue;
            }
        } else if (info->stage == DETECT_LOCK_FINISH) {
            bit_clr(hw_config, HW_485_LOCK1);
            bit_clr(hw_config, HW_485_RELAY1);
            bit_clr(hw_config, HW_485_LOCK2);
            bit_clr(hw_config, HW_485_RELAY2);

            for (i=0; i<MAX_485_IFS; i++) {
                lock = info->lock_485[i];
                relay = info->relay_485[i];
                if (!bit_is_set(info->probe_info[i].mask, LOCK485_MASK_BIT)) {
                    if (lock) {
                        syslog_debug("Lock%d quit\n", i+1);
                        lock->Quit(lock);
                        info->lock_485[i] = NULL;
                    }
                } else {
                    bit_set(hw_config, HW_485_LOCK1+i);
                    lock->Lock_Status_Change_Callback(lock, info->lock_cb);
                    syslog_debug("Lock%d detected\n", i+1);
                }
                
                if (!bit_is_set(info->probe_info[i].mask, RELAY485_MASK_BIT)) {
                    if (relay) {
                        syslog_debug("Relay%d quit\n", i+1);
                        relay->Quit(relay);
                        info->relay_485[i] = NULL;
                    }
                } else {
                    bit_set(hw_config, HW_485_RELAY1+i);
                    relay->Lock_Status_Change_Callback(relay, info->lock_cb);
                    syslog_debug("Relay%d detected\n", i+1);
                }
            }

            if (bit_is_set(hw_config, HW_485_LOCK1) || bit_is_set(hw_config, HW_485_LOCK2)) {
                Lock485_Start();
            }
            if (bit_is_set(hw_config, HW_485_RELAY1) || bit_is_set(hw_config, HW_485_RELAY2)) {
                Relay485_Start();
            }

            for (i=0; i<MAX_485_IFS; i++) {
                if (info->probe_info[i].mask == 0) {
                    bit_set(hw_config, HW_485_MISC1+i);
                    uart485[i]->Set_Rx_Callback(uart485[i], _misc_cb, (void *)info);
                }
            }
            info->stage++;
        } 
        if (hw_config != get_input_reg(UREG3X_HW_CONFIG)) {
            syslog_debug("Update hw_config 0x%x\n", hw_config);
            update_input_reg(UREG3X_HW_CONFIG, hw_config);
        }
        if (info->stage >= DETECT_FINISHED) {
            syslog_debug("Misc485 detect finish\n");
            break;
        } else {
            probe_type++;
            probe_type = probe_type % MAX_DEVICE_TYPES;
            etimer_set(&etimer, 800);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
        }
    }
    
    PROCESS_END();
}

static int _add_callback(Misc485_T *this, Lock_Callback lock_cb, misc485_cb cb)
{
    misc485_info_t *info;

    info = container_of(this, misc485_info_t, public);
    info->lock_cb = lock_cb;
    info->misc_cb = cb;

    return 0;
}

static int _get_lock(Misc485_T *this, int lock_id, int *type, void **lock)
{
    misc485_info_t *info;

    info = container_of(this, misc485_info_t, public);
    if (lock_id < MAX_485_IFS && bit_is_set(info->probe_info[lock_id].mask, LOCK485_MASK_BIT)) {
        *type = LOCK_TYPE_485;
        *lock = info->lock_485[lock_id];
        return 0;
    } else if (bit_is_set(info->probe_info[0].mask, RELAY485_MASK_BIT) && lock_id >= 0 && lock_id < 4) {
        *type = LOCK_TYPE_RELAY;
        *lock = info->relay_485[0];
        return 0;
    } else if (bit_is_set(info->probe_info[1].mask, RELAY485_MASK_BIT) && lock_id >= 0 && lock_id < 4) {
        *type = LOCK_TYPE_RELAY;
        *lock = info->relay_485[1];
        return 0;
    }
    *type = NO_LOCK;
    return 1;
}

static void _open_lock(Misc485_T *this, int lock_id)
{
    misc485_info_t *info;
    Lock_485_T *lock;
    Relay_485_T *relay;
    int status, type;
    void *pp;
    void open_emlock(uint16_t timeout);

    info = container_of(this, misc485_info_t, public);
    status = _get_lock(&info->public, lock_id, &type, &pp);
    if (status == 0) {
        if (type == LOCK_TYPE_485) {
            lock = (Lock_485_T *)pp;
            lock->Open_Door(lock);
            return;
        } else if (type == LOCK_TYPE_RELAY) {
            relay = (Relay_485_T *)pp;
            relay->Open_Door(relay, lock_id);
            return;
        }
    } else {
        // 继电器和485锁都没挂上来，尝试开关板卡上的继电器
        open_emlock(get_hold_reg(UREG4X_EMLOCK_TIMEOUT));
    }
    syslog_error("Open lock error, status:%d, type:%d\n", status, type);
} 

static void _close_lock(Misc485_T *this, int lock_id)
{
    misc485_info_t *info;
    Lock_485_T *lock;
    Relay_485_T *relay;
    int status, type;
    void *pp;    
    void close_emlock(uint16_t timeout);

    info = container_of(this, misc485_info_t, public);
    status = _get_lock(&info->public, lock_id, &type, &pp);
    if (status == 0) {
        if (type == LOCK_TYPE_485) {
            lock = (Lock_485_T *)pp;
            lock->Close_Door(lock);
            return;
        } else if (type == LOCK_TYPE_RELAY) {
            relay = (Relay_485_T *)pp;
            relay->Close_Door(relay, lock_id);
            return;
        }
    } else {
        // 继电器和485锁都没挂上来，尝试开关板卡上的继电器
        close_emlock(get_hold_reg(UREG4X_EMLOCK_TIMEOUT));
    }
    syslog_error("Open lock error, status:%d, type:%d\n", status, type);
    
}

static void _set_delay(Misc485_T *this, uint8_t delay)
{
    int type;
    int status;
    void *lock_p;
    Lock_485_T *lock;
    Relay_485_T *relay;
    int i;
    
    for (i=0; i<2; i++) {
        status = _get_lock(this, i, &type, &lock_p);
        if (status == 0) {
            if (type == LOCK_TYPE_485) {
                lock = (Lock_485_T *)lock_p;
                lock->Set_Delay(lock, delay);
            } else if (type == LOCK_TYPE_RELAY) {
                relay = (Relay_485_T *)lock_p; 
                relay->Set_Delay(relay, delay);
            }
        } else {
            break;
        }
    }
}


Misc485_T *Create_Misc485(Uart_T *uart1, Uart_T *uart2)
{
    uint8_t lock_delay;
    probe_msg_t *probe_msg;
    int status;
    misc485_info_t *misc485_info;
    Misc485_T *public;
    int i;

    zmalloc(misc485_info, sizeof(misc485_info_t));

    uart485[0] = uart1;
    uart485[1] = uart2;
    misc485_info->stage = DETECT_START;
    probe_msg = misc485_info->probe_msg;
    
    
    lock_delay = get_hold_reg(UREG4X_LOCK_DELAY);
    if (lock_delay == 0) {   
        lock_delay = 5;
    }

    for (i=0; i<MAX_485_IFS; i++) {
        misc485_info->lock_485[i] = Create_Lock485(uart485[i], lock_delay);
        i_assert(misc485_info->lock_485[i]);

        misc485_info->relay_485[i] = Create_Relay485(uart485[i], lock_delay);
        i_assert(misc485_info->relay_485[i]);

        uart485[i]->Set_Rx_Callback(uart485[i], _misc_probe_rx, misc485_info);
        misc485_info->probe_info[i].uart = uart485[i];
        misc485_info->probe_info[i].mask = 0;
    }


    // 利用lock1和relay1句柄拿到消息，和lockx是无关的
    status = misc485_info->lock_485[0]->Probe(misc485_info->lock_485[0], &probe_msg[LOCK_485].buf, &probe_msg[LOCK_485].len);
    status |= misc485_info->relay_485[0]->Probe(misc485_info->relay_485[0], &probe_msg[LOCK_RELAY].buf, &probe_msg[LOCK_RELAY].len);

    i_assert(status == 0);

    public = &misc485_info->public;
    public->Add_Callback = _add_callback;
    public->Close_Lock = _close_lock;
    public->Open_Lock = _open_lock;
    public->Set_Delay = _set_delay;
    public->Get_Lock = _get_lock;

    process_start(&_misc485_detect, (const char *)misc485_info);
    
    return public;
}



