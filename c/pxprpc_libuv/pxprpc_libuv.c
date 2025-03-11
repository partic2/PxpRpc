


#include <pxprpc_libuv.h>

struct _pxprpc_libuv{
    uv_loop_t *loop;
    uv_stream_t *listener;
    pxprpc_funcmap *funcmap;
    int status;
    struct _pxprpc_libuv_sockconn *acceptedConn;
};
#define status_SUSPEND 1
#define status_RUNNING 2

struct pxprpc_server_api *servapi;

static int __uvApiInited=0;

static const char *errormsg=NULL;

static pxprpc_server_libuv pxprpc_new_libuvserver(uv_loop_t *loop,uv_stream_t *listener,pxprpc_funcmap *funcmap){
    if(__uvApiInited==0){
        __uvApiInited=1;
        pxprpc_server_query_interface(&servapi);
    }
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)pxprpc__malloc(sizeof(struct _pxprpc_libuv));
    if(loop==NULL){
        loop=listener->loop;
    }
    self->loop=loop;
    self->listener=listener;
    self->funcmap=funcmap;
    self->acceptedConn=NULL;
    return self;
}
/* XXX: How large it should be?*/
#define __sockconn_internalBuf_len 1024
struct _pxprpc_libuv_sockconn{
    struct _pxprpc_libuv *serv;
    struct _pxprpc_libuv_sockconn *prev;
    struct _pxprpc_libuv_sockconn *next;
    struct pxprpc_abstract_io io1;
    union{
        uv_tcp_t tcp;
        uv_pipe_t pipe;
    } stream;
    pxprpc_server_context rpcCtx;
    void (*readCb)(void *);
    void *readCbArgs;
    const char* readError;
    const char* writeError;
    struct pxprpc_buffer_part *nextPxpBuf;
    uint32_t packetRemain;
    uv_buf_t nextBuf;
    char internalBuf[__sockconn_internalBuf_len];
    uint16_t bufStart;
    uint16_t bufEnd;
    int status;
};

#define __sockconn_status_uvreading 1
#define __sockconn_status_streamClosed 2

static void __sockMoveInternalBufToNextBuf(struct _pxprpc_libuv_sockconn *self){
    if(self->nextBuf.len>0){
        int size=self->bufEnd-self->bufStart;
        if(size>self->nextBuf.len){
            size=self->nextBuf.len;
        }
        memmove(self->nextBuf.base,self->internalBuf+self->bufStart,size);
        self->nextBuf.len-=size;
        self->nextBuf.base+=size;
        self->bufStart+=size;
    }
}

static void __sockUvAllocCb(uv_handle_t* handle,size_t suggested_size,uv_buf_t* buf){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn*)uv_handle_get_data(handle);
    if(self->nextBuf.len==0){
        if(self->bufStart==self->bufEnd){
            self->bufStart=0;
            self->bufEnd=0;
        }
        buf->base=self->internalBuf+self->bufEnd;
        buf->len=__sockconn_internalBuf_len-self->bufEnd;
    }else{
        *buf=self->nextBuf;
    }
}
static void __sockAbsIo1ReceivePartCheckAndFlip(struct _pxprpc_libuv_sockconn *self){
    if(self->nextBuf.len==0){
        if(self->nextPxpBuf!=NULL){
            if(self->nextPxpBuf->next_part==NULL){
                /* last part, fill remain data */
                self->nextPxpBuf->bytes.length=self->packetRemain;
                if(self->packetRemain>0){
                    self->nextPxpBuf->bytes.base=pxprpc__malloc(self->packetRemain);
                }
            }
            self->nextBuf.base=self->nextPxpBuf->bytes.base;
            self->nextBuf.len=self->nextPxpBuf->bytes.length;
            self->packetRemain-=self->nextBuf.len;
            self->nextPxpBuf=self->nextPxpBuf->next_part;
            /* to skip zero length buffer */
            __sockAbsIo1ReceivePartCheckAndFlip(self);
        }else{
            if(self->readCb!=NULL){
                void (*readCb)(void*);
                readCb=self->readCb;
                self->readCb=NULL;
                readCb(self->readCbArgs);
            }
        }
    }
}
static void __sockUvReadCb(uv_stream_t* stream,ssize_t nread,const uv_buf_t* buf){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn*)uv_handle_get_data((uv_handle_t *)stream);
    if(nread>0){
        if(buf->base==self->nextBuf.base){
            self->nextBuf.base+=nread;
            self->nextBuf.len-=nread;
        }else{
            /*read to internal buffer*/
            self->bufEnd+=nread;
            __sockMoveInternalBufToNextBuf(self);
        }
        __sockAbsIo1ReceivePartCheckAndFlip(self);
    }else if(nread<0){
        self->readError=uv_strerror(nread);
        uv_read_stop((uv_stream_t *)&self->stream);
        self->nextBuf.base=NULL;
        self->nextPxpBuf=NULL;
        if(self->readCb!=NULL){
            void (*readCb)(void*);
            readCb=self->readCb;
            self->readCb=NULL;
            readCb(self->readCbArgs);
        }
    }
}

static void __sockAbsIo1Receive(struct pxprpc_abstract_io *self1,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_libuv_sockconn *self=(void *)self1-offsetof(struct _pxprpc_libuv_sockconn,io1);
    if(self->nextPxpBuf!=NULL){
        self->readError="overlap read is not allowed";
        onCompleted(p);
        return;
    }
    self->nextPxpBuf=buf;
    struct pxprpc_buffer_part *curbuf=buf;
    uint32_t packetSize=0;
    self->nextBuf.base=(char *)&self->packetRemain;
    self->nextBuf.len=4;
    self->readCb=onCompleted;
    self->readCbArgs=p;
    self->readError=NULL;
    __sockMoveInternalBufToNextBuf(self);
    __sockAbsIo1ReceivePartCheckAndFlip(self);
    if(!(self->status&__sockconn_status_uvreading)){
        uv_read_start((uv_stream_t *)&self->stream,__sockUvAllocCb,__sockUvReadCb);
        self->status|=__sockconn_status_uvreading;
    }
}

struct __sockUvWriteReq{
    uv_write_t uvReq;
    void (*onCompleted)(void *args);
    void *cbArgs;
    struct _pxprpc_libuv_sockconn *sockconn;
    uint32_t packetSize;
    /* following with uv_buf_t array */
};
void __sockUvWriteCb(uv_write_t *req, int status){
    struct __sockUvWriteReq *writeReq=uv_req_get_data((uv_req_t *)req);
    if(status<0){
        writeReq->sockconn->writeError=uv_strerror(status);
    }
    void (*onCompleted)(void *args);
    void *cbArgs;
    onCompleted=writeReq->onCompleted;
    cbArgs=writeReq->cbArgs;
    pxprpc__free(writeReq);
    onCompleted(cbArgs);
}
static void __sockAbsIo1Send(struct pxprpc_abstract_io *self1,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    struct _pxprpc_libuv_sockconn *self=(void *)self1-offsetof(struct _pxprpc_libuv_sockconn,io1);
    int bufCnt=0;
    int packetSize=0;
    struct pxprpc_buffer_part *curbuf=buf;
    while(curbuf!=NULL){
        bufCnt++;
        packetSize+=curbuf->bytes.length;
        curbuf=curbuf->next_part;
    }
    /*reserve for packet size */
    bufCnt++;
    struct __sockUvWriteReq *writeReq=(struct __sockUvWriteReq *)pxprpc__malloc(sizeof(struct __sockUvWriteReq)+sizeof(uv_buf_t)*bufCnt);
    memset(writeReq,0,sizeof(struct __sockUvWriteReq)+sizeof(uv_buf_t)*bufCnt);
    writeReq->packetSize=packetSize;
    uv_buf_t *bufsbase=(void *)writeReq+sizeof(struct __sockUvWriteReq);
    bufsbase[0].base=(char *)&writeReq->packetSize;
    bufsbase[0].len=4;
    curbuf=buf;
    for(int i=1;i<bufCnt;i++){
        bufsbase[i].base=curbuf->bytes.base;
        bufsbase[i].len=curbuf->bytes.length;
        curbuf=curbuf->next_part;
    }
    writeReq->onCompleted=onCompleted;
    writeReq->cbArgs=p;
    uv_write(&writeReq->uvReq,(uv_stream_t *)&self->stream,bufsbase,bufCnt,__sockUvWriteCb);
    uv_req_set_data((uv_req_t *)&writeReq->uvReq,writeReq);
}
static const char *__sockAbsIo1GetError(struct pxprpc_abstract_io *self1,void *fn){
    struct _pxprpc_libuv_sockconn *self=(void *)self1-offsetof(struct _pxprpc_libuv_sockconn,io1);
    if(fn==self1->receive){
        return self->readError;
    }else if(fn==self1->send){
        return self->writeError;
    }
}

static void __freeLibuvSockconn2(uv_handle_t *stream);
static void __sockAbsIo1Close(struct pxprpc_abstract_io *self1){
    struct _pxprpc_libuv_sockconn *self=(void *)self1-offsetof(struct _pxprpc_libuv_sockconn,io1);
    if(!(self->status&__sockconn_status_streamClosed)){
        self->status|=__sockconn_status_streamClosed;
        uv_close((uv_handle_t *)&self->stream,&__freeLibuvSockconn2);
    }
}
static void __freeLibuvSockconn(struct _pxprpc_libuv_sockconn *sc);
static struct _pxprpc_libuv_sockconn * __buildLibuvSockconn(struct _pxprpc_libuv *sCtx){
    struct _pxprpc_libuv_sockconn *sockconn=pxprpc__malloc(sizeof(struct _pxprpc_libuv_sockconn));
    memset(sockconn,0,sizeof(struct _pxprpc_libuv_sockconn));
    sockconn->io1.send=&__sockAbsIo1Send;
    sockconn->io1.receive=&__sockAbsIo1Receive;
    sockconn->io1.get_error=&__sockAbsIo1GetError;
    sockconn->io1.close=&__sockAbsIo1Close;
    sockconn->io1.buf_free=&pxprpc__free;
    memset(&sockconn->stream,0,sizeof(uv_stream_t));
    sockconn->readError=NULL;
    sockconn->writeError=NULL;
    servapi->context_new(&sockconn->rpcCtx,&sockconn->io1);
    struct pxprpc_server_context_exports *ctxexp=servapi->context_exports(sockconn->rpcCtx);
    ctxexp->funcmap=sCtx->funcmap;
    ctxexp->cb_data=sockconn;
    servapi->context_exports_apply(sockconn->rpcCtx);
    if(sCtx->acceptedConn==NULL){
        sCtx->acceptedConn=sockconn;
        sockconn->prev=NULL;
        sockconn->next=NULL;
    }else{
        sCtx->acceptedConn->prev=sockconn;
        sockconn->prev=NULL;
        sockconn->next=sCtx->acceptedConn;
        sCtx->acceptedConn=sockconn;
    }
    sockconn->status=0;
    sockconn->serv=sCtx;
    return sockconn;
}

static void __freeLibuvSockconn(struct _pxprpc_libuv_sockconn *sc){
    if(sc->next!=NULL){
        sc->next->prev=sc->prev;
    }
    if(sc->prev!=NULL){
        sc->prev->next=sc->next;
    }
    //sc->serv==NULL when server is detached.
    if(sc->serv!=NULL && sc->serv->acceptedConn==sc){
        sc->serv->acceptedConn=sc->next;
    }
    servapi->context_delete(&sc->rpcCtx);
    /* sc->stream is closed by io->close() before pxprpc__free */
    pxprpc__free(sc);
}
static void __freeLibuvSockconn2(uv_handle_t *stream){
    struct _pxprpc_libuv_sockconn *sc=uv_handle_get_data(stream);
    __freeLibuvSockconn(sc);
}

static void __uvListenConnectionCb(uv_stream_t *server, int status){
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)uv_handle_get_data((uv_handle_t *)server);
    struct _pxprpc_libuv_sockconn *sockconn=__buildLibuvSockconn(self);
    if(server->type==UV_TCP){
        uv_tcp_init(self->loop, &sockconn->stream.tcp);
    }else if(server->type==UV_NAMED_PIPE){
        uv_pipe_init(self->loop,&sockconn->stream.pipe,0);
    }else{
        fprintf(stderr,"Unknown stream type");
        abort();
    }
    int r=uv_accept(server,(uv_stream_t *)&sockconn->stream);
    if(r<0){
        errormsg="libuv accept error";
        return;
    }
    uv_handle_set_data((uv_handle_t *)&sockconn->stream,sockconn);
    servapi->context_start(sockconn->rpcCtx);
    
}
static void pxprpc_serve(pxprpc_server_libuv serv){
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)serv;
    self->status|=status_RUNNING;
    /*XXX:Can we use user data of listener?*/
    uv_handle_set_data((uv_handle_t *)self->listener,self);
    uv_listen(self->listener,8,__uvListenConnectionCb);
}

static int pxprpc_server_delete(pxprpc_server_libuv serv){
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)serv;
    struct _pxprpc_libuv_sockconn *conn2=NULL;
    //Detach and closing connection.
    for(struct _pxprpc_libuv_sockconn *conn=self->acceptedConn;conn!=NULL;conn=conn2){
        conn2=conn->next;
        conn->serv=NULL;
        conn->io1.close(&conn->io1);
    }
    pxprpc__free(self);
    return 0;
}


static const char *pxprpc_libuv_get_error(){
    return errormsg;
}

static void pxprpc_libuv_clean_error(){
    errormsg=NULL;
}

static pxprpc_libuv_api exports={
    &pxprpc_new_libuvserver,&pxprpc_serve,&pxprpc_server_delete,&pxprpc_libuv_get_error,&pxprpc_libuv_clean_error
};

extern int pxprpc_libuv_query_interface(pxprpc_libuv_api **outapi){
    *outapi=&exports;
}



