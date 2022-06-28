#ifndef __CODE_H__
#define __CODE_H__

enum err {
    STATUS_OK,
    SYS_CALL,
    INIT_ERR = 100,
    INIT_ERR_T1 ,
    INIT_ERR_T2 ,
    PARAM_ERR,
    ITEM_NOT_FOUND,
    ITEM_EXIST,
    NO_SPACE_ERR,
    SYS_CALL_ERR,
    PARSER_ERR,
    OPERATOR_ERR,
    CRC_ERROR, //110
    SYS_BUSY,
    BUS_ERR,
    TRANSFER_ERR,
    COMM_ERROR,
    TIMER_NOT_SUPPORT_ERR,
    OVERFLOW_ERR,
    STATE_ERR,
};



#endif
