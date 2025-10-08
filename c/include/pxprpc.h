
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

struct pxprpc_bytes{
    uint32_t length;
    void *base;
};

struct pxprpc_buffer_part{
    struct pxprpc_bytes bytes;
    struct pxprpc_buffer_part *next_part;
};


struct pxprpc_abstract_io{
    /* 
        Receive a packet. onCompleted is called when buffer is fulfilled or error occured. 
        the buffer memory(buf->base) should free by caller, using io->buf_free(buf->base);
        IO implement should keep buf->base align to 32 bits, To avoid BUS ERROR.
    */
    void (*receive)(struct pxprpc_abstract_io *self,struct pxprpc_bytes *buf,void (*onCompleted)(void *args),void *p);

    /* Send a packet ,onCompleted is called when buffer is processed and can be free or error occured. 
    Write request should be processed in order. */
    void (*send)(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p);

    /* Depending on how implemention allocate buffer on memory. should be "free", if implemention use "malloc". SA:(*receive) */
    void (*buf_free)(void *buf_base);
    
    /* get the last receive/send error. NULL if no error */
    const char *receive_error;
    const char *send_error;

    void (*close)(struct pxprpc_abstract_io *self);

    void *user_data;
    void *backend_data;
};


struct pxprpc_serializer{
    uint8_t *buf;
    uint32_t cap;
    uint32_t pos;
};

extern void pxprpc_ser_prepare_ser(struct pxprpc_serializer *ser,int initBufSize);
extern void pxprpc_ser_prepare_unser(struct pxprpc_serializer *ser,void *buf,uint32_t size);
extern void pxprpc_ser_free_buf(void *buf);
extern void pxprpc_ser_put_int(struct pxprpc_serializer *ser,int32_t *val,int count);
extern void pxprpc_ser_put_long(struct pxprpc_serializer *ser,int64_t *val,int count);
extern void pxprpc_ser_put_float(struct pxprpc_serializer *ser,float *val,int count);
extern void pxprpc_ser_put_double(struct pxprpc_serializer *ser,double *val,int count);
extern void pxprpc_ser_put_varint(struct pxprpc_serializer *ser,uint32_t val);
extern void pxprpc_ser_put_bytes(struct pxprpc_serializer *ser,uint8_t *val,uint32_t size);
extern void pxprpc_ser_get_int(struct pxprpc_serializer *ser,int32_t *val,int count);
extern void pxprpc_ser_get_long(struct pxprpc_serializer *ser,int64_t *val,int count);
extern void pxprpc_ser_get_float(struct pxprpc_serializer *ser,float *val,int count);
extern void pxprpc_ser_get_double(struct pxprpc_serializer *ser,double *val,int count);
extern void pxprpc_ser_get_varint(struct pxprpc_serializer *ser,uint32_t *val);
extern uint8_t *pxprpc_ser_get_bytes(struct pxprpc_serializer *ser,uint32_t *size);
extern uint8_t *pxprpc_ser_build(struct pxprpc_serializer *ser,uint32_t *size);


typedef void *pxprpc_server_context;


typedef struct _pxprpc_ref_s{
    void *object;
    void (*on_free)(struct _pxprpc_ref_s *self);
    /* internal memeber for pxprpc */
    struct _pxprpc_ref_s *nextFree;
}pxprpc_ref;

typedef struct _pxprpc_request_s{
    uint32_t session;
    int32_t callable_index;
    pxprpc_server_context server_context;
    /* parameter hold the parameter byte buffer. If callable want to own the buffer and prevent pxprpc freeing the buffer,
     set parameter.base=NULL */
    struct pxprpc_bytes parameter;
    /* callable can set the value to free resource(like .result member etc.) on request finishing */
    void (*on_finish)(struct _pxprpc_request_s *self);
    struct pxprpc_buffer_part result;
    /* if error occured */
    char rejected;
    /* for callable implemention */
    void *callable_data;
    /* "next_step"  should be called by callable when task finished. */
    void (*next_step)(struct _pxprpc_request_s *r);
    /* internal memeber for pxprpc */
    struct _pxprpc_request_s *nextReq;
    struct _pxprpc_request_s *lastReq;
    struct pxprpc_buffer_part sendBuf;
    int32_t temp1;
    char inSequence;
} pxprpc_request;



typedef struct _pxprpc_callable_s{
    void (*call)(struct _pxprpc_callable_s *self,pxprpc_request *r);
    void *user_data;
}pxprpc_callable;


typedef struct pxprpc_funcmap_s{
    pxprpc_callable *(*get)(struct pxprpc_funcmap_s *self,char *funcname,int namelen);
    void *p;
} pxprpc_funcmap;

struct pxprpc_server_context_exports{
    pxprpc_ref *ref_pool;
    pxprpc_ref *free_ref_entry;
    uint16_t ref_pool_size;
    char closed;
    const char *last_error;
    /* io is READONLY, Modify it may cause unexpected behaviour */
    struct pxprpc_abstract_io *io;
    pxprpc_funcmap *funcmap;
    void *user_data;
    void *cb_data;
    void (*on_closed)(void *cb_data);
};

const char *pxprpc_server_new(pxprpc_server_context *server_context,struct pxprpc_abstract_io *io1);
const char *pxprpc_server_start(pxprpc_server_context server_context);
const char *pxprpc_server_close(pxprpc_server_context server_context);
const char *pxprpc_server_delete(pxprpc_server_context *server_context);
pxprpc_ref *pxprpc_server_alloc_ref(pxprpc_server_context context,uint32_t *index);
void pxprpc_server_free_ref(pxprpc_server_context context,pxprpc_ref *ref);
pxprpc_ref *pxprpc_server_get_ref(pxprpc_server_context context,uint32_t index);
struct pxprpc_server_context_exports *pxprpc_server_get_exports(pxprpc_server_context context);
/* Change in exports may don't take effect immediately, use context_exports_apply to make it.*/
const char *pxprpc_server_exports_apply(pxprpc_server_context context);


#endif