
## rpc term

### serializable data type

There are some serializable data type listed below will be transported as serialized format.

int,long,float,double,bytes,string,varint

and other type will be represented as a reference and store into a reference pool. transported as index in the reference pool.

### reference pool

Each server context has a auto-grow reference pool to hold the reference, and can alloc, free reference in the pool.


## rpc message frame format

The rpc message format is defined like below.

```c

#include <stdint.h>

#pragma pack(1)


struct pxprpc_requst_format{
    uint32_t session_id;
    //refer to the callable reference
    int32_t callable_index;
    char data[remain];
}

struct pxprpc_request_response{
    // the session_id of response should equal to the corresponding request session_id.
    // or resonse.session_id=request.session^0x80000000 if error occured.
    uint32_t session_id;
    char data[remain];
}

//built in base function

//getFunc:get the function named 'function_name'
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_inde=-1;
    char function_name[remain];
}
struct pxprpc_request_response{
    uint32_t session_id;
    //-1 if function not found
    int32_t callable_index;
}

//freeRef:free the reference
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_inde=-2;
    uint32_t index;
}
struct pxprpc_request_response{
    uint32_t session_id;
}


//close:free the resource and disconnect
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_inde=-3;
    uint32_t index;
}
//closed, No response.

//function:getInfo (get string encoded in utf8 indicate the information about the server.)
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_inde=-4;
}
struct pxprpc_request_response{
    uint32_t session_id;
    char server_info[remain];
}
/*
"data" stored the information about the server (as utf8). An example "data" show below
-----------------------------
server name:pxprpc for java
version:2.0
-------------------------------
"server name" indicate the server name.
"version" indicate the pxprpc protocol version. 
*/


/* function:sequence. Requests with same sid(session&0xffffff00) ,
which match the sessionMask, will be executed in sequence (executed after last queued request finished). , since version 2.0.
*/
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_inde=-5;
    uint32_t sessionMask;
}
struct pxprpc_request_response{
    uint32_t session_id;
}

/*
about session mask
if(sessionMask==0xffffffff){
    //no session mask, all session treat as mismatched.
}else{
    maskBitsCnt=sessionMask&0xff;
    maskPattern=sessionMask>>(32-maskBitsCnt);
}
Mask is matched when (session>>(32-maskBitsCnt))==maskPattern
*/

```
