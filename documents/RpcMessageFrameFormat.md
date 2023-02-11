
### rpc message frame format

The rpc message format is defined like below.

```c

#include <stdint.h>

#pragma pack(1)

//function:push (push bytes data to server and put the reference into slot addressed by dest_addr.)
//Explain in C, *dest_addr=&data
struct push_request{
    struct{
        uint8_t opcode;  // "opcode" identify the type of the request. for push_request,this value is 1
        uint8_t id1;
        uint16_t id2;
    } session;   /* "session" is an identifier established by the client.
	The Server MUST reply "session" with the same value in the Response struct for ALL request types */
	
    uint32_t dest_addr;
    uint32_t length;
    char data[length];
};
struct push_response{
    struct{
        uint8_t opcode;  // 1
        uint8_t id1;
        uint16_t id2;
    } session;  // MUST be the same as the value of "session" in request
};

//function:pull (pull bytes data refered by the reference in slot addressed by src_addr.)
//Explain in C, return **src_addr
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
//function:assign (set slot value.)
//Explain in C, *dest_addr=*src_addr
struct assign_request{
    struct{
        uint8_t opcode;  // 3
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t dest_addr;
    uint32_t src_addr;
};
struct assign_response{
    struct{
        uint8_t opcode;  // 3
        uint8_t id1;
        uint16_t id2;
    } session;
};

//function:unlink (set slot to NULL.)
// Explain in C, *dest_addr=NULL
//server may decide if the resources can be free.
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

//function:call (invoke function stored in func_addr.)
//Explain in C, *dest_addr=(*func_addr)(param1,param2...)
//PxpRpc assume the client always know the definition of the function.
//So wrong parameter may BREAK the PxpRpc connection permanently.
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
    any returnValue
};
//In general(but not necessary), return value are defined like below.
struct call_response{
    struct{
        uint8_t opcode;  // 5
        uint8_t id1;
        uint16_t id2;
    } session;
    #if FUNCTION_RETURN_32BIT
    //function return boolean,int32,float32.
    //if function is supposed to return an object, 
    //returnValue=0 to indicate the sucess(In this case, dest_addr will store the return object), 
    //returnValue=1 to indicate the fault(In this case, dest_addr will store an Exception).
    uint32_t returnValue;
    #elif FUNCTION_RETURN_64BIT
    //funcion return int64,float64
    uint64_t returnValue;
    #endif
};

//function:getFunc (get builtin function named by string refered by func_name_addr.) 
//Explain in C,*dest_addr=getFunction(*func_name_addr)
//func_name_addr can be a slot where stored the reference to a utf8 encode string pushed previously.
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

//function:close (free the resource and prepare to disconnect)
struct close_request{
    struct{
        uint8_t opcode;  // 7
        uint8_t id1;
        uint16_t id2;
    } session;
};
//closed, No response.

//function:getInfo (get string encoded in utf8 indicate the information about the server.)
struct getInfo_request{
    struct{
        uint8_t opcode;  // 8
        uint8_t id1;
        uint16_t id2;
    } session;
};
struct getInfo_response{
    struct{
        uint8_t opcode;  // 8
        uint8_t id1;
        uint16_t id2;
    } session;
    uint32_t length; // 0XFFFFFFFF(-1) if data can't be transfered,In this case, sizeof(data)==0
    char data[length];
};
/*
"data" stored the information about the server (as utf8). An example "data" show below
-----------------------------
server name:pxprpc for java
version:1.0
reference slots size:256
-------------------------------
"server name" indicate the server name.
"version" indicate the pxprpc protocol version. Currently only 1.0 is valid.
"reference slots capacity" indicate how many slots can client use. Client should ensure that the slot address is less than this value.
*/

/* 
the server should take responsibility for the lifecycle manager of the objects referred by references slots.  
When an object have no reference refer to, server should free it.
*/
```
