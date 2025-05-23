/* pxprpc runtime bridge is mainly about communicate between runtime in one process. */




#include <stdint.h>
#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <uv.h>

#include <pxprpc_rtbridge.h>

static uv_loop_t *rtbloop;


struct _rtbasyncreq{
    struct _rtbasyncreq *next;
    void (*fn)(void *);
    void *p;
};

static struct _rtbasyncreq *_rtbasyncqueue;

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
    /* receive 1, send 2, connect 3, listen 4, accept 5*/
    char rs;
    struct pxprpc_buffer_part *buf;
    uv_sem_t sem;
    const char *err;
    char *servname;
};


static void _rtbiocb(struct _rtbioreq *req){
    if(req->rs==1){
        req->err=req->io->get_error(req->io,req->io->receive);
    }else if(req->rs==2){
        req->err=req->io->get_error(req->io,req->io->send);
    }
    uv_sem_post(&req->sem);
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

static char *_rtbioreqdispatch(struct _rtbioreq *req){
    uv_sem_init(&req->sem,0);
    pxprpc_pipe_executor((void (*)(void *))_rtbiohandle,req);
    uv_sem_wait(&req->sem);
    uv_sem_destroy(&req->sem);
    return (char *)req->err;
}

char *pxprpc_rtbridge_brecv(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf){
    struct _rtbioreq req;
    req.io=io;
    req.buf=buf;
    req.rs=1;
    return _rtbioreqdispatch(&req);
}


char *pxprpc_rtbridge_bsend(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf){
    struct _rtbioreq req;
    req.io=io;
    req.buf=buf;
    req.rs=2;
    return _rtbioreqdispatch(&req);
}

struct pxprpc_abstract_io *pxprpc_rtbridge_pipe_connect(char *servname){
    struct _rtbioreq req;
    req.io=NULL;
    req.servname=servname;
    req.rs=3;
    _rtbioreqdispatch(&req);
    return req.io;
}


char *pxprpc_rtbridge_init_uv(void *uvloop){
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
static int _rtbinitandrunstat=0;
static void _newloopthread(void *loop){
    if(loop!=NULL){
        _rtbinitandrunstat=1;
        pxprpc_pipe_executor(_newloopthread,NULL);
        uv_run(loop,UV_RUN_DEFAULT);
    }else{
        if(_rtbinitandrunstat==1){
            _rtbinitandrunstat=2;
        }
    }
}
char *pxprpc_rtbridge_init_and_run(void **uvloop){
    if (rtbloop != NULL) {
        return "inited";
    }
    uv_loop_t *newloop=(uv_loop_t *)pxprpc__malloc(sizeof(uv_loop_t));
    uv_loop_init(newloop);
    char *r=pxprpc_rtbridge_init_uv(newloop);
    if(r!=NULL){
        uv_loop_close(newloop);
        pxprpc__free(newloop);
        return r;
    }else{
        *uvloop=newloop;
        uv_thread_create(&_newloopttid,&_newloopthread,newloop);
        for(int i=0;i<100;i++){
            if(_rtbinitandrunstat==2){
                return NULL;
            }
            uv_sleep(16);
        }
        return "uv thread timeout";
    }
}

char *pxprpc_rtbridge_deinit(){
    if(rtbloop!=NULL){
        uv_mutex_destroy(&_rtbmutex);
        uv_close((uv_handle_t *)&_rtbwakeup,NULL);
        rtbloop=NULL;
    }
    if(_rtbinitandrunstat!=0){
        uv_loop_close(rtbloop);
        pxprpc__free(rtbloop);
        rtbloop=NULL;
        _rtbinitandrunstat=0;
    }
    return NULL;
}

void pxprpc_rtbridge_get_current_state(struct pxprpc_rtbridge_state *out){
    out->uv_loop=rtbloop;
    out->uv_tid=&_newloopttid;
    out->run_state=_rtbinitandrunstat;
}