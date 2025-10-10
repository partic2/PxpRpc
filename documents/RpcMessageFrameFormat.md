
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
    char parameter[remain];
}

struct pxprpc_request_response{
    // the session_id of response should equal to the corresponding request session_id.
    // or resonse.session_id=request.session^0x80000000 if error occured.
    uint32_t session_id;
    char parameter[remain];
}

//built in base function

//getFunc:Get the function named 'function_name'
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_index=-1;
    char function_name[remain];x
}
struct pxprpc_request_response{
    uint32_t session_id;
    //-1 if function not found
    int32_t callable_index;
}

//freeRef:Free the reference(s)
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_index=-2;
    uint32_t index[remain];
}
struct pxprpc_request_response{
    uint32_t session_id;
}


//close:Free the resource and disconnect
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_index=-3;
    uint32_t index;
}
//closed, No response.

//getInfo:Get string encoded in utf8 indicate the information about the server.
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_index=-4;
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


/* poll: Call "poll_callable_index" recursively, Until "poll_callable_index" throw a error(resonse.session_id=request.session^0x80000000). */
struct pxprpc_requst_format{
    uint32_t session_id;
    int32_t callable_index=-5;
    int32_t poll_callable_index;
    char parameter[remain];
}
struct pxprpc_request_response{
    /* session_id will be the same for each call. Usually used in event polling. */
    uint32_t session_id;
}


```
