
#ifndef _PXPRPC_H
#define _PXPRPC_H

/* config */
#define pxprpc__malloc malloc
#define pxprpc__free free
#define pxprpc__realloc realloc

/* definition */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

struct pxprpc_abstract_io{
    //onCompleted is called when expected length data is read or error occured.
    void (*read)(struct pxprpc_abstract_io *self,uint32_t length,uint8_t *buf,void (*onCompleted)(void *args),void *p);
    //onCompleted is called when buf is processed and can be free or error occured. 
    //Write request should be processed in order. 
    void (*write)(struct pxprpc_abstract_io *self,uint32_t length,const uint8_t *buf,void (*onCompleted)(void *args),void *p);
    //get last error. For example, to get error caused by read, call "io->get_error(io,io->read)". return NULL if no error
    const char *(*get_error)(struct pxprpc_abstract_io *self,void *fn);
    void *userData;
};

struct pxprpc_bytes{
    uint32_t length;
    //variable length array
    uint8_t data[1];
};

struct pxprpc_serializer{
    uint8_t *buf;
    uint32_t cap;
    uint32_t pos;
};

extern void pxprpc_ser_prepare_ser(struct pxprpc_serializer *ser,int initBufSize);
extern void pxprpc_ser_prepare_unser(struct pxprpc_serializer *ser,void *buf,uint32_t size);
extern void pxprpc_ser_put_int(struct pxprpc_serializer *ser,int32_t *val,int count);
extern void pxprpc_ser_put_long(struct pxprpc_serializer *ser,int64_t *val,int count);
extern void pxprpc_ser_put_float(struct pxprpc_serializer *ser,float *val,int count);
extern void pxprpc_ser_put_double(struct pxprpc_serializer *ser,double *val,int count);
extern void pxprpc_ser_put_bytes(struct pxprpc_serializer *ser,uint8_t *val,uint32_t size);
extern void pxprpc_ser_get_int(struct pxprpc_serializer *ser,int32_t *val,int count);
extern void pxprpc_ser_get_long(struct pxprpc_serializer *ser,int64_t *val,int count);
extern void pxprpc_ser_get_float(struct pxprpc_serializer *ser,float *val,int count);
extern double pxprpc_ser_get_double(struct pxprpc_serializer *ser,double *val,int count);
extern uint8_t *pxprpc_ser_get_bytes(struct pxprpc_serializer *ser,uint32_t *size);
extern uint8_t *pxprpc_ser_build(struct pxprpc_serializer *ser,uint32_t *size);


typedef void *pxprpc_server_context;

struct pxprpc_object{
    void *object1;
    uint32_t (*addRef)(struct pxprpc_object *self);
    uint32_t (*release)(struct pxprpc_object *self);
    //count is used by object internally to save the reference count,
    //pxprpc server only use 'addRef' and 'release'. 
    uint16_t count;
    //type is used to identify the object type.
    //For pxprpc internal object 'pxprpc_bytes' ,type=1.
    //type<16 are reserved by pxprpc for future use,
    //type>=16 are user-defined types.
    uint16_t type;
};

struct pxprpc_request{
    struct pxprpc_abstract_io *io1;
    struct pxprpc_object **ref_slots;
    uint32_t dest_addr;
    uint32_t session;
    struct pxprpc_callable *callable;
    void *callable_data;
    pxprpc_server_context server_context;
    struct pxprpc_object *result;
};


struct pxprpc_callable{
    void (*readParameter)(struct pxprpc_callable *self,struct pxprpc_request *r,void (*doneCallback)(struct pxprpc_request *r));
    void (*call)(struct pxprpc_callable *self,struct pxprpc_request *r,void (*onResult)(struct pxprpc_request *r,struct pxprpc_object *result));
    void (*writeResult)(struct pxprpc_callable *self,struct pxprpc_request *r);
    void *userData;
};

struct pxprpc_namedfunc{
    const char *name;
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
    void (*fill_bytes_object)(struct pxprpc_object *obj,uint8_t *data,int size);
}pxprpc_server_api;

extern int pxprpc_server_query_interface(pxprpc_server_api **outapi);


#endif