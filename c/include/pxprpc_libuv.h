
#ifndef _PXPRPC_LIBUV_H
#define _PXPRPC_LIBUV_H

#include <pxprpc.h>
#include <uv.h>

typedef void *pxprpc_server_libuv;

pxprpc_server_libuv pxprpc_libuv_server_new(uv_loop_t *loop,uv_stream_t *serv,pxprpc_funcmap *funcmap);
const char *pxprpc_libuv_server_start(pxprpc_server_libuv serv);
const char *pxprpc_libuv_server_delete(pxprpc_server_libuv serv);

#endif