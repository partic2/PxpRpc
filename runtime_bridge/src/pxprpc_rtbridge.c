/* pxprpc runtime bridge is mainly about communicate between runtime in one process. */




#include <stdint.h>
#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <uv.h>

#include <pxprpc_rtbridge.h>

static uv_loop_t *rtbloop;

struct _rtbasyncreq{
    uv_async_t uv;
    void (*fn)(void *);
    void *p;
};

void _rtbioasyncentry(uv_async_t *asy){
    struct _rtbasyncreq *r=(struct _rtbasyncreq *)uv_handle_get_data((uv_handle_t *)asy);
    r->fn(r->p);
    pxprpc__free(r);
}

static void pxprpc_rtbridge_async_exec(void (*fn)(void *),void *p){
    struct _rtbasyncreq *r=(struct _rtbasyncreq *)pxprpc__malloc(sizeof(struct _rtbasyncreq));
    uv_async_init(rtbloop,&r->uv,_rtbioasyncentry);
    uv_handle_set_data((uv_handle_t *)&r->uv,r);
    uv_async_send(&r->uv);
}


struct _rtbioreq{
    struct _rtbioreq *next;
    struct pxprpc_abstract_io *io;
    /* receive 1, send 2, connect 3, listen 4, accept 5*/
    char rs;
    char done;
    struct pxprpc_buffer_part *buf;
    uv_mutex_t mut;
    uv_cond_t cond;
    const char *err;
    char *servname;
};

static struct _rtbioreq *_rtbqueued;


static void _rtbiocb(struct _rtbioreq *req){
    if(req->rs==1){
        req->err=req->io->get_error(req->io,req->io->receive);
    }else if(req->rs==2){
        req->err=req->io->get_error(req->io,req->io->send);
    }
    uv_mutex_lock(&req->mut);
    req->done=1;
    uv_cond_broadcast(&req->cond);
    uv_mutex_unlock(&req->mut);
}

static void _rtbiohandle(struct _rtbioreq *req){
    req->err=NULL;
    if(req->rs==1){
        req->io->receive(req->io,req->buf,(void (*)(void *))_rtbiocb,req);
    }else if(req->rs==2){
        req->io->send(req->io,req->buf,(void (*)(void *))_rtbiocb,req);
    }else{
        req->io=pxprpc_pipe_connect(req->servname);
        _rtbiocb(req);
    }
}

char *_rtbioreqdispatch(struct _rtbioreq *req){
    uv_mutex_init(&req->mut);
    uv_cond_init(&req->cond);
    pxprpc_pipe_executor((void (*)(void *))_rtbiohandle,req);
    uv_mutex_lock(&req->mut);
    if(!req->done){
        uv_cond_wait(&req->cond,&req->mut);
    }
    uv_mutex_unlock(&req->mut);
    uv_cond_destroy(&req->cond);
    uv_mutex_destroy(&req->mut);
    return (char *)req->err;
}

char *pxprpc_rtbridge_brecv(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf){
    struct _rtbioreq req;
    req.io=io;
    req.buf=buf;
    req.done=0;
    req.rs=1;
    return _rtbioreqdispatch(&req);
}


char *pxprpc_rtbridge_bsend(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf){
    struct _rtbioreq req;
    req.io=io;
    req.buf=buf;
    req.done=0;
    req.rs=2;
    return _rtbioreqdispatch(&req);
}

struct pxprpc_abstract_io *pxprpc_rtbridge_pipe_connect(char *servname){
    struct _rtbioreq req;
    req.io=NULL;
    req.servname=servname;
    req.done=0;
    req.rs=3;
    _rtbioreqdispatch(&req);
    return req.io;
}

/* constant name for runtime bridge "/pxprpc/runtime_bridge/0", can use pxprpc_pipe_connect to connect */
const char *pxprpc_rtbridge_rtbmanager_name="/pxprpc/runtime_bridge/0";

static void _rtbmgrserve(){

}

/* init rtbridge with libuv */
void pxprpc_rtbridge_init_uv(void *uvloop){
    rtbloop=uvloop;
    if(pxprpc_pipe_executor==NULL && uvloop!=NULL){
        pxprpc_pipe_executor=pxprpc_rtbridge_async_exec;
    }
   pxprpc_pipe_executor(_rtbmgrserve,NULL);
}

