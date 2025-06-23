/* pxprpc runtime bridge is mainly about communicate between runtime in one process. */




#include <stdint.h>
#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <uv.h>

#include <pxprpc_rtbridge.h>

static uv_loop_t *rtbloop=NULL;


struct _rtbasyncreq{
    struct _rtbasyncreq *next;
    void (*fn)(void *);
    void *p;
};

static struct _rtbasyncreq *_rtbasyncqueue=NULL;

static uv_async_t _rtbwakeup;
static uv_mutex_t _rtbmutex;

static void _rtbasyncb(uv_async_t *asy){
    struct _rtbasyncreq *queue;
    uv_mutex_lock(&_rtbmutex);
    queue=_rtbasyncqueue;
    _rtbasyncqueue=NULL;
    uv_mutex_unlock(&_rtbmutex);
    while(queue!=NULL){
        struct _rtbasyncreq *r=queue;
        queue=r->next;
        r->fn(r->p);
        pxprpc__free(r);
    }
}

static void pxprpc_rtbridge_async_exec(void (*fn)(void *),void *p){
    struct _rtbasyncreq *r=(struct _rtbasyncreq *)pxprpc__malloc(sizeof(struct _rtbasyncreq));
    r->fn=fn;
    r->p=p;
    uv_mutex_lock(&_rtbmutex);
    r->next=_rtbasyncqueue;
    _rtbasyncqueue=r;
    uv_mutex_unlock(&_rtbmutex);
    uv_async_send(&_rtbwakeup);
}


struct _rtbioreq{
    struct pxprpc_abstract_io *io;
    /* receive 1, send 2, connect 3, listen 4(Not implemented), accept 5(Not implemented), stop loop 6*/
    char rs;
    union{
       struct pxprpc_buffer_part *send;
       struct pxprpc_bytes *recv;
    } buf;
    uv_sem_t sem;
    const char *err;
    const char *servname;
};


static void _rtbiocb(struct _rtbioreq *req){
    if(req->rs==1){
        req->err=req->io->receive_error;
    }else if(req->rs==2){
        req->err=req->io->send_error;
    }
    uv_sem_post(&req->sem);
}

static void _rtbiohandle(struct _rtbioreq *req){
    req->err=NULL;
    switch(req->rs){
        case 1:
        req->io->receive(req->io,req->buf.recv,(void (*)(void *))_rtbiocb,req);
        break;
        case 2:
        req->io->send(req->io,req->buf.send,(void (*)(void *))_rtbiocb,req);
        break;
        case 3:
        req->io=pxprpc_pipe_connect(req->servname);
        _rtbiocb(req);
        break;
        case 6:
        uv_stop(rtbloop);
        uv_sem_post(&req->sem);
        break;
    }
}

static const char *_rtbioreqdispatch(struct _rtbioreq *req){
    uv_sem_init(&req->sem,0);
    pxprpc_pipe_executor((void (*)(void *))_rtbiohandle,req);
    uv_sem_wait(&req->sem);
    uv_sem_destroy(&req->sem);
    return req->err;
}

const char *pxprpc_rtbridge_brecv(struct pxprpc_abstract_io *io,struct pxprpc_bytes *buf){
    struct _rtbioreq req;
    req.io=io;
    req.buf.recv=buf;
    req.rs=1;
    return _rtbioreqdispatch(&req);
}


const char *pxprpc_rtbridge_bsend(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf){
    struct _rtbioreq req;
    req.io=io;
    req.buf.send=buf;
    req.rs=2;
    return _rtbioreqdispatch(&req);
}

struct pxprpc_abstract_io *pxprpc_rtbridge_pipe_connect(const char *servname){
    struct _rtbioreq req;
    req.io=NULL;
    req.servname=servname;
    req.rs=3;
    _rtbioreqdispatch(&req);
    return req.io;
}


const char *pxprpc_rtbridge_init_uv(void *uvloop){
    if(rtbloop==NULL){
        rtbloop=uvloop;
        uv_mutex_init(&_rtbmutex);
        uv_async_init(rtbloop,&_rtbwakeup,&_rtbasyncb);
        if(pxprpc_pipe_executor==NULL){
            pxprpc_pipe_executor=pxprpc_rtbridge_async_exec;
        }
        return NULL;
    }else{
        return "inited";
    }
}



static uv_thread_t _newloopttid;
static const char *_lastRtbErr=NULL;
static volatile int _rtbinitandrunstat=0;

static void _uvclosenoop(uv_handle_t *arg0){};
static void _uvwalkhandles(uv_handle_t* handle, void* arg) {
    if (!uv_is_closing(handle)) {
        uv_close(handle, _uvclosenoop);
    }
}
static void _newloopthread(void *sem){
    if(rtbloop!=NULL){
        _lastRtbErr="inited";
        uv_sem_post((uv_sem_t *)sem);
        return;
    }
    uv_loop_t* uvloop=(uv_loop_t *)pxprpc__malloc(sizeof(uv_loop_t));
    uv_loop_init(uvloop);
    _lastRtbErr=pxprpc_rtbridge_init_uv(uvloop);
    if(_lastRtbErr!=NULL){
        uv_loop_close(rtbloop);
        pxprpc__free(rtbloop);
        rtbloop=NULL;
        uv_sem_post((uv_sem_t *)sem);
        sem=NULL;
    }else{
        _rtbinitandrunstat=2;
        uv_sem_post((uv_sem_t *)sem);
        sem=NULL;
        uv_run(rtbloop,UV_RUN_DEFAULT);
        pxprpc_pipe_executor=NULL;
        _rtbinitandrunstat=3;
        uv_walk(rtbloop,_uvwalkhandles,NULL);
        uv_run(rtbloop,UV_RUN_DEFAULT);
        uv_loop_close(rtbloop);
        rtbloop=NULL;
        _rtbinitandrunstat=0;
    }
}

const char *pxprpc_rtbridge_init_and_run(void **uvloop){
    if (rtbloop != NULL) {
        return "inited";
    }
    _lastRtbErr=NULL;
    uv_sem_t asyncInitDone;
    uv_sem_init(&asyncInitDone,0);
    uv_thread_create(&_newloopttid,&_newloopthread,&asyncInitDone);
    uv_sem_wait(&asyncInitDone);
    uv_sem_destroy(&asyncInitDone);
    const char *lastErr=_lastRtbErr;
    _lastRtbErr=NULL;
    if(uvloop!=NULL){
        *uvloop=rtbloop;
    }
    return lastErr;
}

const char *pxprpc_rtbridge_deinit(){
    struct _rtbioreq stopreq;
    stopreq.rs=6;
    _rtbioreqdispatch(&stopreq);
    return NULL;
}

void pxprpc_rtbridge_get_current_state(struct pxprpc_rtbridge_state *out){
    out->uv_loop=rtbloop;
    out->uv_tid=&_newloopttid;
    out->run_state=_rtbinitandrunstat;
}