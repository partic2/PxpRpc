
#ifndef _PXPRPC_SERVER_LIBUV_H
#define _PXPRPC_SERVER_LIBUV_H

#include <pxprpc/server.h>
#include <tbox/tbox.h>

typedef void *pxprpc_server_tbox;

typedef struct pxprpc_tbox_api{
    pxprpc_server_tbox (*new_server)(tb_socket_ref_t sock,struct pxprpc_namedfunc *namedFunc,int lenOfNamedFunc);
    void (*serve_block)(pxprpc_server_tbox tbox);
    int (*delete_server)(pxprpc_server_tbox serv);
    const char *(*get_error)();
    void (*clear_error)();
}pxprpc_tbox_api;

extern int pxprpc_tbox_query_interface(pxprpc_tbox_api **outapi);

#endif