
#ifndef _PXPRPC_LIBUV_H
#define _PXPRPC_LIBUV_H

#include <pxprpc.h>
#include <uv.h>

typedef void *pxprpc_server_libuv;

typedef struct pxprpc_libuv_api{
    pxprpc_server_libuv (*new_server)(uv_loop_t *loop,uv_stream_t *serv,pxprpc_funcmap *funcmap);
    void (*serve_start)(pxprpc_server_libuv serv);
    int (*delete_server)(pxprpc_server_libuv serv);
    const char *(*get_error)();
    void (*clear_error)();
}pxprpc_libuv_api;

extern const char *pxprpc_libuv_query_interface(pxprpc_libuv_api **outapi);

#endif