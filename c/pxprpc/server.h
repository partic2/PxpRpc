
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

extern void pxprpc_close(pxprpc_server_context server_context);
extern struct pxprpc_object *pxprpc_new_object(void *obj);
extern struct pxprpc_object *pxprpc_new_bytes_object(uint32_t size);
extern int pxprpc_new_server_context(pxprpc_server_context *server_context,struct pxprpc_abstract_io *io1,
                                     struct pxprpc_namedfunc *namedfuncs,int len_namedfuncs);
extern int pxprpc_start_serve(pxprpc_server_context server_context);
extern int pxprpc_free_context(pxprpc_server_context *server_context);

#endif