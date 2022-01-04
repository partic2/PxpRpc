
#ifndef _PXPRPC_SERVER_H
#define _PXPRPC_SERVER_H

#include <stdint.h>

#include "def.h"

#include "config.h"

#pragma pack(1)

typedef void *pxprpc_server_context;

struct pxprpc_object{
    void *object1;
    uint16_t size_of_struct;
    uint16_t count;
    uint32_t (*addRef)(struct pxprpc_object *ref);
    uint32_t (*release)(struct pxprpc_object *ref);
};

struct pxprpc_request{
    struct pxprpc_abstract_io *io1;
    struct pxprpc_object **ref_slots;
    uint32_t dest_addr;
    uint32_t session;
    struct pxprpc_callable *callable;
    void *callable_data;
    void *server_context_data;
    struct pxprpc_object *result;
};


struct pxprpc_callable{
    void (*readParameter)(struct pxprpc_callable *self,struct pxprpc_request *r,void (*doneCallback)(struct pxprpc_request *r));
    void (*call)(struct pxprpc_callable *self,struct pxprpc_request *r,void (*onResult)(struct pxprpc_request *r,struct pxprpc_object *result));
    void (*writeResult)(struct pxprpc_callable *self,struct pxprpc_request *r);
};

struct pxprpc_namedfunc{
    char *name;
    struct pxprpc_callable *callable;
};

typedef struct pxprpc_server_api{
    int (*context_new)(pxprpc_server_context *server_context,struct pxprpc_abstract_io *io1,
                                     struct pxprpc_namedfunc *namedfuncs,int len_namedfuncs);
    int (*context_start)(pxprpc_server_context);
    int (*context_closed)(pxprpc_server_context);
    int (*context_close)(pxprpc_server_context);
    int (*context_delete)(pxprpc_server_context *);
    struct pxprpc_object *(*new_object)(void *obj);
    struct pxprpc_object *(*new_bytes_object)(uint32_t size);
}pxprpc_server_api;

extern int pxprpc_query_interface(pxprpc_server_api **outapi);

#endif