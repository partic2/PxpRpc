/* pxprpc runtime bridge is mainly about communicate between runtime in one process. */




#include <stdint.h>
#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <uv.h>

#include <pxprpc_rtbridge.h>

#ifdef __linux__
#include <signal.h>
#endif


struct _rtbasyncreq{
    struct _rtbasyncreq *next;
    void (*fn)(void *);
    void *p;
};



struct _rtbasyncreqqueue{
    uv_thread_t thread;
    uv_mutex_t mutex;
    struct _rtbasyncreq *queuehead;
    uv_async_t wakeup;
    uv_loop_t *uvloop;
    uv_sem_t initedSignal;
    const char * volatile lastErr;
};

static void _rtbqueuewakeupcb(uv_async_t *asy){
    struct _rtbasyncreq *req;
    struct _rtbasyncreqqueue *queue1=asy->data;
    uv_mutex_lock(&queue1->mutex);
    req=queue1->queuehead;
    queue1->queuehead=NULL;
    uv_mutex_unlock(&queue1->mutex);
    while(req!=NULL){
        struct _rtbasyncreq *r=req;
        req=r->next;
        r->fn(r->p);
        pxprpc__free(r);
    }
}

void pxprpc_rtbridge_post_executable(void *uv_exec_queue,void (*fn)(void *),void *p){
    struct _rtbasyncreqqueue *queue=(struct _rtbasyncreqqueue *)uv_exec_queue;
    struct _rtbasyncreq *r=(struct _rtbasyncreq *)pxprpc__malloc(sizeof(struct _rtbasyncreq));
    r->fn=fn;
    r->p=p;
    uv_mutex_lock(&queue->mutex);
    r->next=queue->queuehead;
    queue->queuehead=r;
    uv_mutex_unlock(&queue->mutex);
    uv_async_send(&queue->wakeup);
}


static struct _rtbasyncreqqueue *_mainrtbqueue=NULL;

static void pxprpc_rtbridge_async_exec_main_loop(void (*fn)(void *),void *p){
    pxprpc_rtbridge_post_executable(_mainrtbqueue,fn,p);
}

struct _rtbioreq{
    struct pxprpc_abstract_io *io;
    /* receive 1, send 2, connect 3, close 4, stop loop 6*/
    char rs;
    union{
       struct pxprpc_buffer_part *send;
       struct pxprpc_bytes *recv;
    } buf;
    /* For send/receive, if post_executable is not null, use post_executable instead of sem， And will also free this _rtbioreq after post_executable return.*/
    void (*post_executable)(void *p,void (*executable)(void *p2),void *p2);
    void *post_executable_p;
    void (*onCompleted)(void *p);
    void *onCompleted_p;
    struct pxprpc_abstract_io *deliveryError;
    uv_sem_t sem;
    const char *err;
    const char *servname;
};


static void _rtbiocb(struct _rtbioreq *req){
    if(req->rs==1){
        req->err=req->io->receive_error;
        if(req->deliveryError!=NULL){
            req->deliveryError->receive_error=req->io->receive_error;
        }
    }else if(req->rs==2){
        req->err=req->io->send_error;
        if(req->deliveryError!=NULL){
            req->deliveryError->send_error=req->io->send_error;
        }
    }
    if(req->post_executable!=NULL){
        req->post_executable(req->post_executable_p,req->onCompleted,req->onCompleted_p);
        pxprpc__free(req);
    }else{
        uv_sem_post(&req->sem);
    }
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
        case 4:
        req->io->close(req->io);
        _rtbiocb(req);
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
    req.deliveryError=NULL;
    req.post_executable=NULL;
    req.io=io;
    req.buf.recv=buf;
    req.rs=1;
    return _rtbioreqdispatch(&req);
}


const char *pxprpc_rtbridge_bsend(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf){
    struct _rtbioreq req;
    req.deliveryError=NULL;
    req.post_executable=NULL;
    req.io=io;
    req.buf.send=buf;
    req.rs=2;
    return _rtbioreqdispatch(&req);
}


struct pxprpc_abstract_io *pxprpc_rtbridge_pipe_connect(const char *servname){
    struct _rtbioreq req;
    req.deliveryError=NULL;
    req.post_executable=NULL;
    req.io=NULL;
    req.servname=servname;
    req.rs=3;
    _rtbioreqdispatch(&req);
    return req.io;
}

void pxprpc_rtbridge_io_close(struct pxprpc_abstract_io *io){
    struct _rtbioreq req;
    req.deliveryError=NULL;
    req.post_executable=NULL;
    req.io=io;
    req.rs=4;
    _rtbioreqdispatch(&req);
    return;
}


static void _uvclosenoop(uv_handle_t *arg0){};
static void _uvwalkhandles(uv_handle_t* handle, void* arg) {
    if (!uv_is_closing(handle)) {
        uv_close(handle, _uvclosenoop);
    }
}

static void _newloopthread(void *queueIn){
    struct _rtbasyncreqqueue *queue=(struct _rtbasyncreqqueue *)queueIn;
    uv_loop_t* uvloop=(uv_loop_t *)pxprpc__malloc(sizeof(uv_loop_t));
    uv_loop_init(uvloop);
    queue->uvloop=uvloop;
    if(uv_async_init(queue->uvloop,&queue->wakeup,&_rtbqueuewakeupcb)<0){
        queue->lastErr="uv_async_init failed.";
        uv_loop_close(queue->uvloop);
        pxprpc__free(queue->uvloop);
        uv_sem_post(&queue->initedSignal);
        return;
    }
    queue->wakeup.data=queue;
    uv_mutex_init(&queue->mutex);
    queue->queuehead=NULL;
    uv_sem_post(&queue->initedSignal);
    uv_run(queue->uvloop,UV_RUN_DEFAULT);
    uv_walk(queue->uvloop,_uvwalkhandles,NULL);
    uv_run(queue->uvloop,UV_RUN_DEFAULT);
    uv_loop_close(queue->uvloop);
    uv_mutex_destroy(&queue->mutex);
    pxprpc__free(queueIn);
}

const char *pxprpc_rtbridge_new_uvthread(void **uvthread_output){
    struct _rtbasyncreqqueue *queue=pxprpc__malloc(sizeof(struct _rtbasyncreqqueue));
    uv_sem_init(&queue->initedSignal,0);
    queue->lastErr=NULL;    
    uv_thread_create(&queue->thread,&_newloopthread,queue);
    uv_sem_wait(&queue->initedSignal);
    uv_sem_destroy(&queue->initedSignal);
    if(queue->lastErr==NULL){
        *uvthread_output=queue;
    }
    return queue->lastErr;
}

static void _rtbStopQueueLoop(void *queueIn){
    struct _rtbasyncreqqueue *queue=queueIn;
    uv_stop(queue->uvloop);
}


const char *pxprpc_rtbridge_stop_uvthread(void **uvthread_inout){
    struct _rtbasyncreqqueue *queue=(struct _rtbasyncreqqueue *)*uvthread_inout;
    if(queue!=NULL){
        pxprpc_rtbridge_post_executable(queue, _rtbStopQueueLoop,queue);
        *uvthread_inout=NULL;
    }
    return NULL;
}


const char *pxprpc_rtbridge_init_and_run(void **uvloop){
    const char *err=NULL;
    #ifdef __linux__
    signal(SIGPIPE, SIG_IGN);
    #endif
    if(_mainrtbqueue==NULL){
       err=pxprpc_rtbridge_new_uvthread((void **)&_mainrtbqueue);
    }
    if(err==NULL){
        *uvloop=_mainrtbqueue->uvloop;
        pxprpc_pipe_executor=&pxprpc_rtbridge_async_exec_main_loop;
    }
    return err;
}


const char *pxprpc_rtbridge_deinit(void){
    return pxprpc_rtbridge_stop_uvthread((void **)_mainrtbqueue);
}



void pxprpc_rtbridge_get_uvthread_state(void *uvthread,struct pxprpc_rtbridge_state *out){
    struct _rtbasyncreqqueue * queue=(struct _rtbasyncreqqueue *)uvthread;
    out->uv_loop=queue->uvloop;
    out->uv_tid=&queue->thread;
    out->uvthread=queue;
}


void pxprpc_rtbridge_get_current_state(struct pxprpc_rtbridge_state *out){
    pxprpc_rtbridge_get_uvthread_state(_mainrtbqueue,out);
}


struct _threadsafe_pxprpc_io{
    struct pxprpc_abstract_io export;
    struct pxprpc_abstract_io *wrapped;
    void (*post_executable)(void *p,void (*executable)(void *p2),void *p2);
};


void _threadsafe_pxprpc_io_receive(struct pxprpc_abstract_io *self2,struct pxprpc_bytes *buf,void (*onCompleted)(void *args),void *p){
    struct _threadsafe_pxprpc_io *self=(struct _threadsafe_pxprpc_io *)self2;
    struct _rtbioreq req;
    req.io=self->wrapped;
    req.buf.recv=buf;
    req.rs=2;
    req.post_executable=self->post_executable;
    req.onCompleted=onCompleted;
    req.onCompleted_p=p;
    req.deliveryError=&self->export;
    pxprpc_pipe_executor((void (*)(void *))_rtbiohandle,&req);
}

void _threadsafe_pxprpc_io_send(struct pxprpc_abstract_io *self2,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    struct _threadsafe_pxprpc_io *self=(struct _threadsafe_pxprpc_io *)self2;
    struct _rtbioreq req;
    req.io=self->wrapped;
    req.buf.send=buf;
    req.rs=1;
    req.post_executable=self->post_executable;
    req.onCompleted=onCompleted;
    req.onCompleted_p=p;
    req.deliveryError=&self->export;
    pxprpc_pipe_executor((void (*)(void *))_rtbiohandle,&req);
}


void _threadsafe_pxprpc_io_close(struct pxprpc_abstract_io *self2){
    struct _threadsafe_pxprpc_io *self=(struct _threadsafe_pxprpc_io *)self2;
    pxprpc_rtbridge_io_close(self->wrapped);
    pxprpc__free(self2);
}


struct pxprpc_abstract_io *pxprpc_rtbridge_wrap_thread(struct pxprpc_abstract_io *io,void (*post_executable)(void *p,void (*executable)(void *p2),void *p2),void *p){
    struct _threadsafe_pxprpc_io *result=(struct _threadsafe_pxprpc_io *)pxprpc__malloc(sizeof(struct _threadsafe_pxprpc_io));
    result->post_executable=post_executable;
    result->wrapped=io;
    result->export.send=&_threadsafe_pxprpc_io_send;
    result->export.receive=&_threadsafe_pxprpc_io_receive;
    result->export.close=&_threadsafe_pxprpc_io_close;
    return &result->export;
}