#include "modbus.h"
#include "heap.h"
#include "log.h"
#include "list.h"
#include "process.h"
#include "etimer.h"

#define T35 10

#define BUF_SIZE 265 //标准RTU最长报文为256字节，维控的会超过一点，稍微开大一些
#define ASCII_BUF_SIZE 533
#define ASCII_TIMEOUT 1000 //ascii帧的字符超时时间
#define ASCII_REQ_MIN_SIZE (9+8) //固定开销9个字节，数据字段至少4个字
#define MASTER_MAX_RETRIES 5
#define MAX_CMD16_REGS 32
#define MODBUS_UART_NUM 8
#define MAX_EXTRA_CB 2

typedef struct {
    uint16_t addr;
    reg_process_cb cb;
} reg_handle_t;

typedef struct {
    list_head_t list;

    Uart_T *uart;
    uint8_t uid;
    uint8_t mode;    
    uint8_t *ascii_buf;    
} uart_conf_t;


typedef struct {
    MBSlave_T public;
    list_head_t conf_list; // conf列表

    struct {
        uint16_t *p;
        uint16_t num;
    } reg[MODBUS_REG_TYPE_NUM];
    uint32_t *cmask;
    //uint32_t *emask; // 有回调的掩码，加速处理
    reg_handle_t *handle;
	reg_process_cb reg_default_cb;
    uint8_t cmask_size;
    uint16_t handle_size;
    ModAddr_Map addr_map;
    struct {
        uint8_t func;
        extra_func_cb cb;
    } extra_func[MAX_EXTRA_CB];
} modbus_info_t;

enum MESSAGE
{
    ID                             = 0, //!< ID field
    FUNC, //!< Function code position
    ADD_HI, //!< Address high byte
    ADD_LO, //!< Address low byte
    NB_HI, //!< Number of coils or registers high byte
    NB_LO, //!< Number of coils or registers low byte
    BYTE_CNT  //!< byte counter
};
enum
{
    NO_REPLY = 255,
    EXC_FUNC_CODE = 1,
    EXC_ADDR_RANGE = 2,
    EXC_REGS_QUANT = 3,
    EXC_EXECUTE = 4
};

enum MB_FC
{
    MB_FC_NONE                     = 0,   /*!< null operator */
    MB_FC_READ_REGISTERS           = 3,	/*!< FCT=3 -> read registers or analog outputs */
    MB_FC_READ_INPUT_REGISTER      = 4,	/*!< FCT=4 -> read analog inputs */
    MB_FC_WRITE_REGISTER           = 6,	/*!< FCT=6 -> write single register */
    MB_FC_WRITE_MULTIPLE_REGISTERS = 16	/*!< FCT=16 -> write multiple registers */
};

enum
{
    RESPONSE_SIZE = 6,
    EXCEPTION_SIZE = 3,
    CHECKSUM_SIZE = 2
};

static void _slave_rx_process(Uart_T *uart, void *this, uint8_t *buf, uint16_t size);


static int _set_reg(MBSlave_T *modbus, uint8_t reg_type, uint16_t *reg, uint16_t num)
{
    modbus_info_t *info;
    int i;
    
    i_assert(reg_type < MODBUS_REG_TYPE_NUM);

    info = container_of(modbus, modbus_info_t, public);
    info->reg[reg_type].p = reg;
    info->reg[reg_type].num = num;

    if (reg_type == MODBUS_HOLDING_REG) {
        if (num > MAX_REG_MONITOR) {
            num = MAX_REG_MONITOR; // 最多支持0~511寄存器的写回调
        }
        info->cmask_size = (num + 31) / 32; // 向上取整
        
        zmalloc(info->cmask, info->cmask_size * sizeof(uint32_t));
        //zmalloc(info->emask, info->cmask_size * sizeof(uint32_t));
        // 最多50个回调函数
        info->handle_size = num/4 <= 50 ? num/4 : 50;
        if (info->handle_size > 0) {
            zmalloc(info->handle, sizeof(reg_handle_t) * info->handle_size);
            for (i=0; i<info->handle_size; i++) {
                info->handle[i].addr = 0xffff;
            }
        }        
    }
    return 0;
}
static int _add_uart(MBSlave_T *this, Uart_T *uart, uint8_t mode, uint8_t uid)
{
    modbus_info_t *info;
    uart_conf_t *conf;

    if (mode == MODBUS_RTU || mode == MODBUS_ASCII) {
        info = container_of(this, modbus_info_t, public);
        zmalloc(conf, sizeof(uart_conf_t));
        list_add_tail(&conf->list, &info->conf_list);

        conf->mode = mode;
        conf->uid = uid;
        conf->uart = uart;
        uart->Set_Rx_Callback(uart, _slave_rx_process, this);
        
        if (mode == MODBUS_ASCII) {
            zmalloc(conf->ascii_buf, ASCII_BUF_SIZE);
        }
        return 0;
    } else {
        return -PARAM_ERR;
    }
}

static int _register_cb(MBSlave_T *modbus, uint16_t addr, reg_process_cb cb)
{
    int i, j, k;
    modbus_info_t *info;
    reg_handle_t *handle;
    reg_handle_t tmp;
    
    info = container_of(modbus, modbus_info_t, public);
    handle = info->handle;
    //info->emask[addr/32] |= 1 << (addr % 32);
    
    for (i=0; i<info->handle_size; i++) {
        if (handle[i].cb == NULL) {
            break;
        }
    }

    i_assert(i < info->handle_size);
    handle[i].cb = cb;
    handle[i].addr = addr;

    for (j=0; j<i; j++) {
        for (k=0; k<i-j; k++) {
            if (handle[k].addr > handle[k+1].addr) {
                memcpy(&tmp, &handle[k], sizeof(reg_handle_t));
                memcpy(&handle[k], &handle[k+1], sizeof(reg_handle_t));
                memcpy(&handle[k+1], &tmp, sizeof(reg_handle_t));
            }
        }
    }
    return 0;
}

static int _register_default_cb(MBSlave_T *modbus, reg_process_cb cb)
{
    modbus_info_t *info;

	info = container_of(modbus, modbus_info_t, public);
    if(cb != NULL){
        info->reg_default_cb = cb;
        return 0;
    }
    return -1;
}


int _slave_extra_funcb(MBSlave_T *modbus, uint8_t func, extra_func_cb cb)
{
    modbus_info_t *info;
    int i;
    
    info = container_of(modbus, modbus_info_t, public);
    for (i=0; i<MAX_EXTRA_CB; i++) {
        if (info->extra_func[i].func == 0) {
            break;
        }
    }
    if (i < MAX_EXTRA_CB) {
        info->extra_func[i].func = func;
        info->extra_func[i].cb = cb;
        return 0;
    } else {
        return -NO_SPACE_ERR;
    }
}


static uint8_t _c2i(char c)
{
    if (c >= '0' && c <= '9') {
        return c-'0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xa;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xa;
    }  else {
        return 0xFF;
    }
}
static uint8_t _i2c(uint8_t i)
{
    if (i<=9) {
        return i + '0';
    } else if (i >= 0xa & i <= 0xf) {
        return i - 0xa + 'A';
    }
    return 0xff;
}
static uint8_t _a2hex(uint8_t *array)
{
    return (_c2i(array[0]) << 4 )  | _c2i(array[1]);
}

static void _hex2a(uint8_t v, uint8_t *array)
{
    array[0] = _i2c(v >> 4);
    array[1] = _i2c(v & 0xf);
}

static uint16_t _cal_crc(uint8_t *buf, uint16_t size)
{
    uint16_t temp, temp2, flag;
    int i, j;
    
    temp = 0xFFFF;
    for (i = 0; i < size; i++) {
        temp = temp ^ buf[i];
        for ( j = 1; j <= 8; j++) {
            flag = temp & 0x0001;
            temp >>=1;
            if (flag) {
                temp ^= 0xA001;
            }
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;
}

static uint8_t _cal_lrc(uint8_t *buf, uint16_t size)
{
    int i;
    uint8_t lrc = 0;

    for (i=0; i<size; i+=2) {
        lrc += _a2hex(&buf[i]);
    }
    return (uint8_t)(-(int8_t)lrc);
}

static int _build_exception(uint8_t uid, uint8_t *buf, int reason)
{
    uint8_t func = buf[FUNC];
    uint16_t crc;
    int n;

    buf[ID] = uid;
    buf[FUNC] = func + 0x80;
    buf[2] = reason;
    n = EXCEPTION_SIZE;

    crc = _cal_crc(buf, EXCEPTION_SIZE);
    buf[n++] = high_byte(crc);
    buf[n++] = low_byte(crc);
    
    return n;
}
static int _valid_request(modbus_info_t *info, uart_conf_t *conf, uint8_t *buf, uint16_t size)
{
    uint16_t crc_msg, crc_calcu;
    uint16_t min_reg, max_reg, start_reg, end_reg;
    uint8_t type;
    int i;

    if (conf->mode == MODBUS_RTU) {
        crc_msg = word(buf[size - 2], buf[size-1]);
        crc_calcu = _cal_crc(buf, size-2);
        if (crc_msg != crc_calcu) {
            syslog_error("crc error request except is %d, but is %d, size is %d\n", crc_calcu, crc_msg, size);
            for(uint8_t i=0; i<size; i++){
                syslog_debug("%x", buf[i]);
            }
              
            return NO_REPLY;
        }
    }

    switch(buf[FUNC]) {
        case MB_FC_READ_INPUT_REGISTER:
        case MB_FC_READ_REGISTERS:
            start_reg = word(buf[ADD_HI], buf[ADD_LO]);
            end_reg = start_reg + word(buf[NB_HI], buf[NB_HI]);
        break;
        case MB_FC_WRITE_REGISTER:
            start_reg = word(buf[ADD_HI], buf[ADD_LO]);
            end_reg = start_reg;
        break;
        case MB_FC_WRITE_MULTIPLE_REGISTERS:
            start_reg = word(buf[ADD_HI], buf[ADD_LO]);
            end_reg = start_reg + word(buf[NB_HI], buf[NB_LO]);
        break;
        default:
            for (i=0; i<MAX_EXTRA_CB; i++) {
                if (buf[FUNC] == info->extra_func[i].func) {
                    return 0;
                }
            }
            return EXC_FUNC_CODE;
        break;
    }

    if (buf[FUNC] == MB_FC_READ_INPUT_REGISTER) {
        min_reg = 0;
        type = MODBUS_INPUT_REG;
    } else {
        min_reg = 0;
        type = MODBUS_HOLDING_REG;
    }
    
    max_reg = info->reg[type].num;

	for (int i=start_reg; i<end_reg; i++) {
		int j = info->addr_map(type, i);
	    if (j >= max_reg || j < min_reg) {
            return EXC_ADDR_RANGE;
        }
    }
    return 0;
}

static int _process_fc34(modbus_info_t *info, uart_conf_t *conf, uint8_t *buf, uint16_t *reg, uint16_t num)
{
    uint16_t start, no;
    uint16_t crc;
    int i, n;
	int reg_type;
    
    //uid and func code is ready.
    start = word(buf[ADD_HI], buf[ADD_LO]);
    no = word(buf[NB_HI], buf[NB_LO]);

    if (reg == info->reg[MODBUS_INPUT_REG].p) {
		reg_type = MODBUS_INPUT_REG;
    } else {
		reg_type = MODBUS_HOLDING_REG;
    }
	
    buf[2] = no * 2;
    n = 3;

    if (no * 2 + n + 2 >= BUF_SIZE) {
        /*Overflow*/
        return _build_exception(conf->uid, buf, EXC_REGS_QUANT);
    }
    for (i=start; i<start+no; i++) {
		int j = info->addr_map(reg_type, i);
        
        buf[n++] = high_byte(reg[j]);
        buf[n++] = low_byte(reg[j]);
    }
    crc = _cal_crc(buf, n);
    buf[n++] = high_byte(crc);
    buf[n++] = low_byte(crc);

    return n;
}

static int _process_fc6(modbus_info_t *info, uart_conf_t *conf, uint8_t *buf, uint16_t *reg, uint16_t num, uint32_t *cmask, uint16_t *end_addr)
{
    uint16_t addr, val;
    int n;

    //uid and func code is ready.
    addr = word(buf[ADD_HI], buf[ADD_LO]);
    val = word(buf[NB_HI], buf[NB_LO]);

    addr = info->addr_map(MODBUS_HOLDING_REG, addr);
    reg[addr] = val;
    if (addr < MAX_REG_MONITOR) {
        cmask[addr/32] |= 1 << (addr % 32);
    } else {
        *end_addr = addr;
    }

    n = RESPONSE_SIZE + CHECKSUM_SIZE;
    
    return n;
}

static int _process_fc16(modbus_info_t *info, uart_conf_t *conf, uint8_t *buf, uint16_t *reg, uint16_t num, uint32_t *cmask, uint16_t *end_addr)
{
    uint16_t start, no;
    uint16_t crc, v;
    int i, j, n, offset;

    //uid and func code is ready.
    start = word(buf[ADD_HI], buf[ADD_LO]);
    no = word(buf[NB_HI], buf[NB_LO]);
    offset = BYTE_CNT + 1;
    n = RESPONSE_SIZE;

    for (i=0; i<no; i++) {
		j = info->addr_map(MODBUS_HOLDING_REG, start+i);
        v = word(buf[offset + i*2], buf[offset+i*2+1]);
        reg[j] = v;
        if (j < MAX_REG_MONITOR) {
            cmask[j/32] |= 1 << (j % 32);
        }
    }
    if (j >= MAX_REG_MONITOR) {
        *end_addr = j;
    }
    

    crc = _cal_crc(buf, n);
    buf[n++] = high_byte(crc);
    buf[n++] = low_byte(crc);

    return n;
}

static void _do_handle(modbus_info_t *info, uint16_t addr)
{
    int i;

    for (i=0; i<info->handle_size; i++) {
        if (info->handle[i].addr == addr && info->handle[i].cb) {
            info->handle[i].cb(addr);
            if(info->reg_default_cb){
				info->reg_default_cb(addr);
			}
			return;
        } else if (info->handle[i].addr > addr) {
            break;
        }
    }
    if(info->reg_default_cb){
		info->reg_default_cb(addr);
	}
}

static int _recv_rtu_frame(uart_conf_t *conf, uint8_t *buf, uint16_t size)
{
    if (size < BYTE_CNT+2 && buf[1] < 50) {
        if (size != 0) {
            syslog_error("Incomplete Frame:%d %d %d\n", size, buf[1], buf[2]);
        }
    } else if (buf[ID] == conf->uid) {
        return size;
    }
    return 0;
}

static int _ascii2rtu(uint8_t *ascii_buf, uint16_t buf_len, uint8_t *rtu_buf)
{
    int i,j;

    j = 0;
    for (i=1; i<buf_len-4; i+=2) {
        /*跳过头部冒号、尾部lrc和cr lx */
        rtu_buf[j++] = _a2hex(&ascii_buf[i]);
    }
    return j+2;//留出2个字节给crc，实际不计算
    
}
static int _rtu2ascii(uint8_t *rtu_buf, uint16_t len, uint8_t *ascii_buf)
{
    int i, j;
    uint8_t lrc;

    j = 0;
    ascii_buf[j++] = ':';

    for (i=0; i<len-2; i++) {//skip rtu crc field
        _hex2a(rtu_buf[i], &ascii_buf[j]);
        j += 2;
    }
    lrc = _cal_lrc(ascii_buf+1, j-1);
    _hex2a(lrc, &ascii_buf[j]);
    j += 2;

    ascii_buf[j++] = 0xd;
    ascii_buf[j++] = 0xa;
    
    return j;
}

static int _recv_ascii_frame(uart_conf_t *conf, uint8_t *buf, uint16_t size)
{
    uint8_t lrc_exp, lrc;
    uint8_t *ascii_buf;

    if (size > ASCII_BUF_SIZE) {
        syslog_error("Modbus ascii oversize\n");
        return 0;
    }
    ascii_buf = conf->ascii_buf;
    memcpy(ascii_buf, buf, size);
    
    if (size < ASCII_REQ_MIN_SIZE) {
        /*数据不完整，先返回 */
        return 0;
    } else {
        if (ascii_buf[0] != ':') {
            /*不是： */
            syslog_error("Frame Head Error\n");
        } else if (ascii_buf[size - 2] != 0xd || ascii_buf[size - 1] != 0xa) {
           /*没检查到尾，先不处理*/
        } else {
        /*lrc检查*/
            lrc_exp = _cal_lrc(ascii_buf + 1, size - 1 - 4);//跳过头部1字节、crc和帧字节
            lrc = _a2hex(&ascii_buf[size - 4]);
            if (lrc_exp != lrc) {
                syslog_error("LRC error, exp 0x%x, msg 0x%x\n", lrc_exp, lrc);
            } else {
                return _ascii2rtu(ascii_buf, size, buf);
            }
        }
    }
    return 0;
}

static void _slave_rx_process(Uart_T *uart, void *this, uint8_t *buf, uint16_t size)
{
    modbus_info_t *info;
    int status;
    int tx_size;
    int i, j;
    uint8_t written = 0;
    uint16_t addr, end_addr;
    list_head_t *p;
    uart_conf_t *conf;
    
    info = container_of(this, modbus_info_t, public);   
    tx_size = 0;

    list_for_each(p, &info->conf_list) {
        conf = container_of(p, uart_conf_t, list);
        if (conf->uart == uart) {
            break;
        } else {
            conf = NULL;
        }
    }
   

    if (conf == NULL) {
        syslog_error("Can't find uart\n");
        return;
    }

    if (conf->mode == MODBUS_RTU) {
        size = _recv_rtu_frame(conf, buf, size);
    } else {
        size = _recv_ascii_frame(conf, buf, size);
    }

    if (size > 0) {
        status =  _valid_request(info, conf, buf, size);
            /*do process*/
        if (status > 0) {
            if (status != NO_REPLY) {
                tx_size = _build_exception(conf->uid, buf, status);
            }
            goto end;
        }
        end_addr = 0; // >= 512的holding register统一走默认处理函数，处理最后一个寄存器
        
        switch (buf[FUNC]) {
            case MB_FC_READ_INPUT_REGISTER:
                tx_size = _process_fc34(info, conf, buf, info->reg[MODBUS_INPUT_REG].p, 
                    info->reg[MODBUS_INPUT_REG].num);
            break;
            case MB_FC_READ_REGISTERS:
                tx_size = _process_fc34(info, conf, buf, info->reg[MODBUS_HOLDING_REG].p, 
                    info->reg[MODBUS_HOLDING_REG].num);  
            break;
            case MB_FC_WRITE_REGISTER:
                written = TRUE;
                tx_size = _process_fc6(info, conf, buf, info->reg[MODBUS_HOLDING_REG].p, 
                    info->reg[MODBUS_HOLDING_REG].num, info->cmask, &end_addr);
            break;
            case MB_FC_WRITE_MULTIPLE_REGISTERS:
                written = TRUE;
                tx_size = _process_fc16(info, conf, buf, info->reg[MODBUS_HOLDING_REG].p, 
                    info->reg[MODBUS_HOLDING_REG].num, info->cmask, &end_addr);
            break;
            default:
                for (i=0; i<MAX_EXTRA_CB; i++) {
                    if (buf[FUNC] == info->extra_func[i].func && info->extra_func[i].cb) {
                        tx_size = info->extra_func[i].cb(&info->public, buf, size);
                        break;
                    }
                }
                if (i >= MAX_EXTRA_CB) {                
                    syslog_critical("Impossible func code %d", buf[FUNC]);
                }
            break;
        }
    }
end:
    if (tx_size) {
        if (conf->mode == MODBUS_ASCII) {
            tx_size = _rtu2ascii(buf, tx_size, conf->ascii_buf);
            buf = conf->ascii_buf;
        }
        uart->Write_Bytes(uart, buf, tx_size);
    }
    /*do write callback*/
    if (written) {
        uint32_t mask;
        for (i=0; i<info->cmask_size; i++) {
            mask = info->cmask[i]; // & info->emask[i];
            if (mask) {
                for (j=0; j<32; j++) {
                    if (mask & (1<<j)) {
                        addr = (i << 5) | j; // i * 32 | j
                        _do_handle(info, addr);
                        mask &= ~(1<<j);
                        if (mask == 0) {
                            break;
                        }                        
                    }
                }
            }
            info->cmask[i] = 0;
        }
        if (end_addr >= MAX_REG_MONITOR) {
            if(info->reg_default_cb){
        		info->reg_default_cb(end_addr);
        	}
        }
        
    }
}


static int _set_addr_map(MBSlave_T *modbus, ModAddr_Map map_fn)
{
    modbus_info_t *info;

    info = container_of(modbus, modbus_info_t, public);
    info->addr_map = map_fn;

    return 0;
}

static uint16_t _dummy_map(uint8_t type, uint16_t addr)
{
    return addr;
}


MBSlave_T *Create_Modbus_Slave()
{
    modbus_info_t *info;
    MBSlave_T *public;

    zmalloc(info, sizeof(modbus_info_t));
    LIST_HEAD_INIT(&info->conf_list);

    public = &info->public;
    public->Set_Reg = _set_reg;
    public->Add_Uart = _add_uart;
    public->Set_AddrMap = _set_addr_map;
    public->Slave_Reg_ProcessCB = _register_cb;
    public->Slave_ExtraFuncCB = _slave_extra_funcb;
	public->Slave_Reg_Default_ProcessCB = _register_default_cb;
    _set_addr_map(public, _dummy_map);

    return public;
}


