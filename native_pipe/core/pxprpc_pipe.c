#include <pxprpc_pipe.h>
#include <stddef.h>
#include <stdint.h>

typedef struct _s_pxprpcPipeServer{
    const char *name;
    void (*on_connect)(struct pxprpc_abstract_io *,void *p);
    void *p;
    struct _s_pxprpcPipeServer *next;
} _pxprpcPipeServer;

static _pxprpcPipeServer *_pxprpcServerList=NULL;


typedef struct _s_pxpIoReq{
    struct pxprpc_abstract_io *self;
    struct pxprpc_buffer_part *buf;
    void (*onCompleted)(void *args);
    void *p;
    struct _s_pxpIoReq *next;
} _pxpIoReq;

static char *_pxpErrorOverlapedReceive="Overlapped receive is not supported";
const char *pxprpc_pipe_error_connection_closed="Connection closed.";

typedef struct _s_pxprpcPipeConnection{
    struct pxprpc_abstract_io clientSide;
    struct pxprpc_abstract_io serverSide;
    _pxpIoReq *clientSend;
    _pxpIoReq clientRecv;
    _pxpIoReq *serverSend;
    _pxpIoReq serverRecv;
    char closing;
    const char *serverSendError;
    const char *serverRecvError;
    const char *clientSendError;
    const char *clientRecvError;
} _pxprpcPipeConnection;

static void _pxpTransferBuffer(struct pxprpc_buffer_part *providerBuffer,struct pxprpc_buffer_part *consumerBuffer){
    struct pxprpc_buffer_part *tBuff=NULL;
    int providerOffset=0;
    int consumerOffset=0;
    while(providerBuffer!=NULL&&consumerBuffer!=NULL&&consumerBuffer->bytes.length>0){
        int providerRemain=providerBuffer->bytes.length-providerOffset;
        int consumerRemain=consumerBuffer->bytes.length-consumerOffset;
        int moveSize=providerRemain>consumerRemain?consumerRemain:providerRemain;
        memmove(consumerBuffer->bytes.base+consumerOffset,providerBuffer->bytes.base+providerOffset,moveSize);
        providerOffset+=moveSize;
        if(providerOffset==providerBuffer->bytes.length){
            providerBuffer=providerBuffer->next_part;
            providerOffset=0;
        }
        consumerOffset+=moveSize;
        if(consumerOffset==consumerBuffer->bytes.length){
            consumerBuffer=consumerBuffer->next_part;
            consumerOffset=0;
        }
    }
    if(consumerBuffer->bytes.length==0){
        int remain=providerBuffer->bytes.length-providerOffset;
        tBuff=providerBuffer->next_part;
        while(tBuff!=NULL){
            remain+=tBuff->bytes.length;
            tBuff=tBuff->next_part;
        };
        consumerBuffer->bytes.base=pxprpc__malloc(remain);
        consumerBuffer->bytes.length=remain;
        consumerOffset=0;
        while(providerBuffer!=NULL){
            int moveSize=providerBuffer->bytes.length-providerOffset;
            memmove(consumerBuffer->bytes.base+consumerOffset,providerBuffer->bytes.base+providerOffset,moveSize);
            providerBuffer=providerBuffer->next_part;
            consumerOffset+=moveSize;
            providerOffset=0;
        }
    }
}

static void _pxprpcPipePump(_pxprpcPipeConnection *conn){
    void (* onCompleted)(void *args);
    if(conn->clientRecv.onCompleted!=NULL&&conn->serverSend!=NULL){
        _pxpIoReq *req=conn->serverSend;
        _pxpTransferBuffer(req->buf,conn->clientRecv.buf);
        conn->serverSend=req->next;
        onCompleted=conn->clientRecv.onCompleted;
        conn->clientRecv.onCompleted=NULL;
        onCompleted(conn->clientRecv.p);
        req->onCompleted(req->p);
        pxprpc__free(req);
    }
    if(conn->serverRecv.onCompleted!=NULL&&conn->clientSend!=NULL){
        _pxpIoReq *req=conn->clientSend;
        _pxpTransferBuffer(req->buf,conn->serverRecv.buf);
        conn->clientSend=req->next;
        onCompleted=conn->serverRecv.onCompleted;
        conn->serverRecv.onCompleted=NULL;
        onCompleted(conn->serverRecv.p);
        req->onCompleted(req->p);
        pxprpc__free(req);
    }
}

static void _pxprpcPipeConnectionClientSend(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,clientSide));
    conn->clientSendError=NULL;
    if(conn->closing){
        conn->clientSendError=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    _pxpIoReq *newSendReq=pxprpc__malloc(sizeof(_pxpIoReq));
    newSendReq->self=self;
    newSendReq->buf=buf;
    newSendReq->p=p;
    newSendReq->onCompleted=onCompleted;
    newSendReq->next=NULL;
    _pxpIoReq *lastReq=conn->clientSend;
    if(lastReq==NULL){
        conn->clientSend=newSendReq;
    }else{
        while(lastReq->next!=NULL){
            lastReq=lastReq->next;
        }
        lastReq->next=newSendReq;
    }
    _pxprpcPipePump(conn);
}

static void _pxprpcPipeConnectionClientReceive(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,clientSide));
    conn->clientRecvError=NULL;
    if(conn->clientRecv.onCompleted!=NULL){
        conn->clientRecvError=_pxpErrorOverlapedReceive;
        onCompleted(p);
        return;
    }
    if(conn->closing){
        conn->clientRecvError=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    conn->clientRecv.self=self;
    conn->clientRecv.buf=buf;
    conn->clientRecv.onCompleted=onCompleted;
    conn->clientRecv.p=p;
    _pxprpcPipePump(conn);
}


static void _pxprpcPipeConnectionServerSend(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,serverSide));
    conn->serverSendError=NULL;
    if(conn->closing){
        conn->serverSendError=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    _pxpIoReq *newSendReq=pxprpc__malloc(sizeof(_pxpIoReq));
    newSendReq->self=self;
    newSendReq->buf=buf;
    newSendReq->p=p;
    newSendReq->onCompleted=onCompleted;
    newSendReq->next=NULL;
    _pxpIoReq *lastReq=conn->serverSend;
    if(lastReq==NULL){
        conn->serverSend=newSendReq;
    }else{
        while(lastReq->next!=NULL){
            lastReq=lastReq->next;
        }
        lastReq->next=newSendReq;
    }
    _pxprpcPipePump(conn);
}

static void _pxprpcPipeConnectionServerRecv(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,serverSide));
    conn->serverRecvError=NULL;
    if(conn->serverRecv.onCompleted!=NULL){
        conn->serverRecvError=_pxpErrorOverlapedReceive;
        onCompleted(p);
        return;
    }
    if(conn->closing){
        conn->serverRecvError=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    conn->serverRecv.self=self;
    conn->serverRecv.buf=buf;
    conn->serverRecv.onCompleted=onCompleted;
    conn->serverRecv.p=p;
    _pxprpcPipePump(conn);
}

static void _pxprpcPipeConnectionClosing(_pxprpcPipeConnection *conn){
    void (*onCompleted)(void *p);
    if(conn->closing)return;
    conn->closing=1;
    conn->serverRecvError=pxprpc_pipe_error_connection_closed;
    conn->serverSendError=pxprpc_pipe_error_connection_closed;
    conn->clientRecvError=pxprpc_pipe_error_connection_closed;
    conn->clientSendError=pxprpc_pipe_error_connection_closed;
    if(conn->serverRecv.onCompleted!=NULL){
        onCompleted=conn->serverRecv.onCompleted;
        conn->serverRecv.onCompleted=NULL;
        onCompleted(conn->serverRecv.p);
    }
    if(conn->clientRecv.onCompleted!=NULL){
        onCompleted=conn->clientRecv.onCompleted;
        conn->clientRecv.onCompleted=NULL;
        onCompleted(conn->clientRecv.p);
    }
    while(conn->serverSend!=NULL){
        _pxpIoReq *req=conn->serverSend;
        req->onCompleted(req->p);
        conn->serverSend=req->next;
        pxprpc__free(req);
    }
    while(conn->clientSend!=NULL){
        _pxpIoReq *req=conn->clientSend;
        req->onCompleted(req->p);
        conn->clientSend=req->next;
        pxprpc__free(req);
    }
    pxprpc__free(conn);
}

static void _pxprpcPipeConnectionServerClose(struct pxprpc_abstract_io *self){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,serverSide));
    _pxprpcPipeConnectionClosing(conn);
}

static void _pxprpcPipeConnectionClientClose(struct pxprpc_abstract_io *self){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,clientSide));
    _pxprpcPipeConnectionClosing(conn);
}


static const char *_pxprpcPipeConnectionClientGetError(struct pxprpc_abstract_io *self,void *fn){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,clientSide));
    if(fn==self->send){
        return conn->clientSendError;
    }else if(fn==self->receive){
        return conn->clientRecvError;
    }else{
        return NULL;
    }
}


static const char *_pxprpcPipeConnectionServerGetError(struct pxprpc_abstract_io *self,void *fn){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,serverSide));
    if(fn==self->send){
        return conn->serverSendError;
    }else if(fn==self->receive){
        return conn->serverRecvError;
    }else{
        return NULL;
    }
}


void pxprpc_pipe_serve(const char *name,void (*on_connect)(struct pxprpc_abstract_io *,void *p),void *p){
    _pxprpcPipeServer *serv=_pxprpcServerList;
    _pxprpcPipeServer *lastServ=NULL;
    while(serv!=NULL){
        if(strcmp(serv->name,name)==0){
            if(on_connect==NULL){
                if(lastServ==NULL){
                    _pxprpcServerList->next=serv->next;
                }else{
                    lastServ->next=serv->next;
                }
            }else{
                serv->on_connect=on_connect;
            }
            break;
        }
        lastServ=serv;
        serv=serv->next;
    }
    if(serv==NULL){
        serv=pxprpc__malloc(sizeof(_pxprpcPipeServer));
        serv->name=name;
        serv->on_connect=on_connect;
        serv->next=_pxprpcServerList;
        _pxprpcServerList=serv;
    }
}

struct pxprpc_abstract_io *pxprpc_pipe_connect(const char *name){
    _pxprpcPipeServer *serv=_pxprpcServerList;
    while(serv!=NULL){
        if(strcmp(name,serv->name)==0){            
            break;
        }
    }
    if(serv==NULL){
        return NULL;
    }
    _pxprpcPipeConnection *conn=pxprpc__malloc(sizeof(_pxprpcPipeConnection));
    conn->clientRecv.onCompleted=NULL;
    conn->closing=0;
    conn->clientSendError=NULL;
    conn->clientRecvError=NULL;
    conn->clientSend=NULL;
    conn->clientSide.buf_free=&pxprpc__free;
    conn->clientSide.send=&_pxprpcPipeConnectionClientSend;
    conn->clientSide.receive=&_pxprpcPipeConnectionClientReceive;
    conn->clientSide.close=&_pxprpcPipeConnectionClientClose;
    conn->clientSide.get_error=&_pxprpcPipeConnectionClientGetError;

    conn->serverRecv.onCompleted=NULL;
    conn->closing=0;
    conn->serverSendError=NULL;
    conn->serverRecvError=NULL;
    conn->serverSend=NULL;
    conn->serverSide.buf_free=&pxprpc__free;
    conn->serverSide.send=&_pxprpcPipeConnectionServerSend;
    conn->serverSide.receive=&_pxprpcPipeConnectionServerRecv;
    conn->serverSide.close=&_pxprpcPipeConnectionServerClose;
    conn->serverSide.get_error=&_pxprpcPipeConnectionServerGetError;
    serv->on_connect(&conn->serverSide,serv->p);
    return &conn->clientSide;
}
