


#include "server_libuv.h"
#include <pxprpc/server.h>
#include <pxprpc/def.h>

struct _pxprpc_libuv{
    uv_loop_t *loop;
    uv_stream_t *listener;
    struct pxprpc_namedfunc *namedFunc;
    int lenOfNamedFunc;
    int status;
    struct _pxprpc_libuv_sockconn *acceptedConn;
};
#define status_SUSPEND 1
#define status_RUNNING 2

struct pxprpc_server_api *servapi;

int inited=0;

static const char *errormsg=NULL;

static pxprpc_server_libuv pxprpc_new_libuvserver(uv_loop_t *loop,uv_stream_t *listener,struct pxprpc_namedfunc *namedFunc,int lenOfNamedFunc){
    if(inited==0){
        inited=1;
        pxprpc_server_query_interface(&servapi);
    }
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)pxprpc__malloc(sizeof(struct _pxprpc_libuv));
    self->loop=loop;
    self->listener=listener;
    self->lenOfNamedFunc=lenOfNamedFunc;
    self->namedFunc=namedFunc;
    self->acceptedConn=NULL;
    return self;
}
#define __sockconn_internalBuf_len 4096+8
struct _pxprpc_libuv_sockconn{
    struct _pxprpc_libuv *serv;
    struct _pxprpc_libuv_sockconn *prev;
    struct _pxprpc_libuv_sockconn *next;
    struct pxprpc_abstract_io io1;
    uv_tcp_t stream;
    pxprpc_server_context rpcCtx;
    void (*readCb)(void *);
    void *readCbArgs;
    const char* readError;
    const char* writeError;
    uv_buf_t nextBuf;
    char internalBuf[__sockconn_internalBuf_len];
    uint32_t bufStart;
    uint32_t bufEnd;
    int status;
};

#define __sockconn_status_uvreading 1

static void __sockMoveInternalBufToNextBuf(struct _pxprpc_libuv_sockconn *self){
    if(self->nextBuf.len>0){
        if(self->bufEnd<self->bufStart){
            int size=__sockconn_internalBuf_len-self->bufStart;
            if(size>self->nextBuf.len){
                size=self->nextBuf.len;
            }
            memcpy(self->nextBuf.base,self->internalBuf+self->bufStart,size);
            self->nextBuf.len-=size;
            self->bufStart+=size;
            if(self->bufStart>=__sockconn_internalBuf_len){
                self->bufStart-=__sockconn_internalBuf_len;
            }
        }
    }
    if(self->nextBuf.len>0){
        if(self->bufStart<self->bufEnd){
            int size=self->bufEnd-self->bufStart;
            if(size>self->nextBuf.len){
                size=self->nextBuf.len;
            }
            memcpy(self->nextBuf.base,self->internalBuf+self->bufStart,size);
            self->nextBuf.len-=size;
            self->bufStart+=size;
        }
    }
}

static void __sockUvAllocCb(uv_handle_t* handle,size_t suggested_size,uv_buf_t* buf){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn*)uv_handle_get_data(handle);
    if(self->nextBuf.len==0){
        buf->base=self->internalBuf+self->bufEnd;
        if(self->bufStart>self->bufEnd){
            //remain 4 byte to avoid self->bufStart==self->bufEnd, which indicate buffer is empty.
            buf->len=self->bufStart-self->bufEnd-4;
        }else{
            buf->len=__sockconn_internalBuf_len;
            if(self->bufStart<4){
                buf->len-=4-self->bufStart;
            }
        }
    }else{
        *buf=self->nextBuf;
    }
}
static void __sockUvReadCb(uv_stream_t* stream,ssize_t nread,const uv_buf_t* buf){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn*)uv_handle_get_data((uv_handle_t *)stream);
    if(nread>0){
        if(buf->base==self->nextBuf.base){
            self->nextBuf.base+=nread;
            self->nextBuf.len-=nread;
        }else{
            //read to internal buffer
            self->bufEnd+=nread;
            if(self->bufEnd>=__sockconn_internalBuf_len){
                self->bufEnd-=__sockconn_internalBuf_len;
            }
            __sockMoveInternalBufToNextBuf(self);
        }
        if(self->nextBuf.len==0){
            self->nextBuf.base=NULL;
            if(self->readCb!=NULL){
                void (*readCb)(void*);
                readCb=self->readCb;
                self->readCb=NULL;
                readCb(self->readCbArgs);
            }
        }
    }else if(nread<0){
        self->readError="libuv read error";
        uv_read_stop((uv_stream_t *)&self->stream);
        self->nextBuf.base=NULL;
        if(self->readCb!=NULL){
            void (*readCb)(void*);
            readCb=self->readCb;
            self->readCb=NULL;
            readCb(self->readCbArgs);
        }
    }
}
static void __sockAbsIo1Read(struct pxprpc_abstract_io *self1,uint32_t length,uint8_t *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn *)self1->userData;
    if(self->nextBuf.base!=NULL){
        self->readError="overlap read is not allowed";
        onCompleted(p);
        return;
    }
    self->nextBuf.base=buf;
    self->nextBuf.len=length;
    self->readCb=onCompleted;
    self->readCbArgs=p;
    self->readError=NULL;
    __sockMoveInternalBufToNextBuf(self);
    if(self->nextBuf.len==0){
        self->nextBuf.base=NULL;
        if(self->readCb!=NULL){
            void (*readCb)(void*);
            readCb=self->readCb;
            self->readCb=NULL;
            readCb(self->readCbArgs);
        }
    }
    if(!(self->status&__sockconn_status_uvreading)){
        uv_read_start((uv_stream_t *)&self->stream,__sockUvAllocCb,__sockUvReadCb);
        self->status|=__sockconn_status_uvreading;
    }
}

// consider cross thread request;
struct __sockUvWriteReq{
    uv_write_t uvReq;
    uv_buf_t buf;
    void (*onCompleted)(void *args);
    void *cbArgs;
    struct _pxprpc_libuv_sockconn *sockconn;
};
void __sockUvWriteCb(uv_write_t *req, int status){
    struct __sockUvWriteReq *writeReq=uv_req_get_data((uv_req_t *)req);
    if(status<0){
        writeReq->sockconn->writeError="libuv write error";
    }
    writeReq->onCompleted(writeReq->cbArgs);
}
static void __sockAbsIo1Write(struct pxprpc_abstract_io *self1,uint32_t length,const uint8_t *buf,void (*onCompleted)(void *args),void *p){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn *)self1->userData;
    struct __sockUvWriteReq *writeReq=(struct __sockUvWriteReq *)pxprpc__malloc(sizeof(struct __sockUvWriteReq));
    memset(writeReq,0,sizeof(struct __sockUvWriteReq));
    writeReq->buf.base=(char *)buf;
    writeReq->buf.len=length;
    writeReq->onCompleted=onCompleted;
    writeReq->cbArgs=p;
    uv_write(&writeReq->uvReq,(uv_stream_t *)&self->stream,&writeReq->buf,1,__sockUvWriteCb);
    uv_req_set_data((uv_req_t *)&writeReq->uvReq,writeReq);
}
static const char *__sockAbsIo1GetError(struct pxprpc_abstract_io *self1,void *fn){
    struct _pxprpc_libuv_sockconn *self=(struct _pxprpc_libuv_sockconn *)self1->userData;
    if(fn==self1->read){
        return self->readError;
    }else if(fn==self1->write){
        return self->writeError;
    }
}

static struct _pxprpc_libuv_sockconn * __buildLibuvSockconn(struct _pxprpc_libuv *sCtx){
    struct _pxprpc_libuv_sockconn *sockconn=pxprpc__malloc(sizeof(struct _pxprpc_libuv_sockconn));
    sockconn->io1.userData=sockconn;
    sockconn->io1.read=&__sockAbsIo1Read;
    sockconn->io1.write=&__sockAbsIo1Write;
    sockconn->io1.get_error=&__sockAbsIo1GetError;
    sockconn->readCb=NULL;
    sockconn->readCbArgs=NULL;
    sockconn->nextBuf.len=0;
    sockconn->nextBuf.base=NULL;
    sockconn->bufStart=0;
    sockconn->bufEnd=0;
    memset(&sockconn->stream,0,sizeof(uv_stream_t));
    sockconn->readError=NULL;
    sockconn->writeError=NULL;
    servapi->context_new(&sockconn->rpcCtx,&sockconn->io1,sCtx->namedFunc,sCtx->lenOfNamedFunc);
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

static void __uvStreamCloseCb(uv_handle_t* handle){
    struct _pxprpc_libuv_sockconn *sockconn=uv_handle_get_data(handle);
    pxprpc__free(sockconn);
}

static void __freeLibuvSockconn(struct _pxprpc_libuv_sockconn *sc){
    servapi->context_delete(&sc->rpcCtx);
    uv_close((uv_handle_t *)&sc->stream,__uvStreamCloseCb);
    if(sc->next!=NULL){
        sc->next->prev=sc->prev;
    }
    if(sc->prev!=NULL){
        sc->prev->next=sc->next;
    }
    if(sc->serv->acceptedConn==sc){
        sc->serv->acceptedConn=sc->next;
    }
    pxprpc__free(sc);
}

static void __uvListenConnectionCb(uv_stream_t *server, int status){
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)uv_handle_get_data((uv_handle_t *)server);
    struct _pxprpc_libuv_sockconn *sockconn=__buildLibuvSockconn(self);
    uv_tcp_init(self->loop, &sockconn->stream);
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
    //XXX:Can we use user data of listener?
    uv_handle_set_data((uv_handle_t *)self->listener,self);
    uv_listen(self->listener,8,__uvListenConnectionCb);
}

static int pxprpc_server_delete(pxprpc_server_libuv serv){
    struct _pxprpc_libuv *self=(struct _pxprpc_libuv *)serv;
    struct _pxprpc_libuv_sockconn *conn2=NULL;
    for(struct _pxprpc_libuv_sockconn *conn=self->acceptedConn;conn!=NULL;conn=conn2){
        conn2=conn->next;
        __freeLibuvSockconn(conn);
    }
    pxprpc__free(serv);
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



