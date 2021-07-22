# Pxp RPC 

PxpRpc(pursuer cross platform remote procedure call) is a very tiny rpc library aim to call and interchange data cross platform and language with high performance and little load.

Currently , This project is in early state and only implement server and test on Java(>=1.6) platform

the message format is defined like below.


```c

#include <stdint.h>

#pragma pack(1)
//push (push bytes data into slot address dest_addr)
struct push_request{
    struct{
        uint8_t opcode;  // 1
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr;
    uint32_t length;
    char data[length];
};
struct push_response{
    struct{
        uint8_t opcode;  // 1
        uint8_t id1;
        uint16_t id2;
    } session;
};
//pull (pull bytes data from slot address src_addr)
struct pull_request{
    struct{
        uint8_t opcode;  // 2
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t src_addr;
};
struct pull_response{
    struct{
        uint8_t opcode;  // 2
        uint8_t id1;
        uint16_t id2;
    } session;

    uint32_t length; // 0XFFFFFFFF(-1) if data can't be transfered,In this case, sizeof(data)==0
    char data[length];   
};
//assign (set *dest_addr=*src_addr)
struct assign_request{
    struct{
        uint8_t opcode;  // 3
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr;
    uint32_t src_addr;
};
struct aassign_response{
    struct{
        uint8_t opcode;  // 3
        uint8_t id1;
        uint16_t id2;
    } session;
};
//unlink (set *dest_addr=NULL)
struct unlink_request{
    struct{
        uint8_t opcode;  // 4
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr;
};
struct unlink_response{
    struct{
        uint8_t opcode;  // 4
        uint8_t id1;
        uint16_t id2;
    } session;
};
//call (*dest_addr=(*func_addr)(param1,param2...))
struct call_request{
    struct{
        uint8_t opcode;  // 5
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr;
    uint32_t func_addr;
    //parameter
    uint32_t param1;
    uint64_t param2;
    uint32_t etc;
};
struct call_response{
    struct{
        uint8_t opcode;  // 5
        uint8_t id1;
        uint16_t id2;
    } session;
    #if FUNCTION_RETURN_32BIT
    //function return boolean,int8-int32,float32,object slot address
    uint32_t returnValue;
    #elif FUNCTION_RETURN_64BIT
    //funcion return int64,float64
    uint64_t returnValue;
    #endif
};
//getFunc (*dest_addr=getFunction(*func_name_addr)) 
//func_name_addr encode as utf8
struct getFunc_request{
    struct{
        uint8_t opcode;  // 6
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr;
    uint32_t func_name_addr;
};
struct getFunc_response{
    struct{
        uint8_t opcode;  // 6
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr; // 0 if function not found
};

//close (free the resource and prepare to disconnect)
struct close_request{
    struct{
        uint8_t opcode;  // 7
        uint8_t id1;
        uint16_t id2;
    } session;
};
//closed, No response.
```



See java test file for usage.

Feel free to PR and issue

