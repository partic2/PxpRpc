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
    union{
        struct pxprpc_buffer_part *send;
        struct pxprpc_bytes *recv;
    }buf;
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
    char serverClosed;
    char clientClosed;
} _pxprpcPipeConnection;

static void _pxpTransferBuffer(struct pxprpc_buffer_part *providerBuffer,struct pxprpc_bytes *consumerBuffer){
    struct pxprpc_buffer_part *tBuff=NULL;
    consumerBuffer->length=0;
    uint32_t consumeOffset=0;
    for(tBuff=providerBuffer;tBuff!=NULL;tBuff=tBuff->next_part){
        consumerBuffer->length+=tBuff->bytes.length;
    }
    consumerBuffer->base=pxprpc__malloc(consumerBuffer->length);
    for(tBuff=providerBuffer;tBuff!=NULL;tBuff=tBuff->next_part){
        memmove((uint8_t *)consumerBuffer->base+consumeOffset,tBuff->bytes.base,tBuff->bytes.length);
        consumeOffset+=tBuff->bytes.length;
    }
}

static void _pxprpcPipePump(_pxprpcPipeConnection *conn){
    void (* onCompleted)(void *args);
    if(conn->clientRecv.onCompleted!=NULL&&conn->serverSend!=NULL){
        _pxpIoReq *req=conn->serverSend;
        _pxpTransferBuffer(req->buf.send,conn->clientRecv.buf.recv);
        conn->serverSend=req->next;
        onCompleted=conn->clientRecv.onCompleted;
        conn->clientRecv.onCompleted=NULL;
        onCompleted(conn->clientRecv.p);
        req->onCompleted(req->p);
        pxprpc__free(req);
    }
    if(conn->serverRecv.onCompleted!=NULL&&conn->clientSend!=NULL){
        _pxpIoReq *req=conn->clientSend;
        _pxpTransferBuffer(req->buf.send,conn->serverRecv.buf.recv);
        conn->clientSend=req->next;
        onCompleted=conn->serverRecv.onCompleted;
        conn->serverRecv.onCompleted=NULL;
        onCompleted(conn->serverRecv.p);
        req->onCompleted(req->p);
        pxprpc__free(req);
    }
}

static void _pxprpcPipeConnectionClientSend(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)((char *)self-(offsetof(_pxprpcPipeConnection,clientSide)));
    conn->clientSide.send_error=NULL;
    if(conn->clientClosed || conn->serverClosed){
        conn->clientSide.send_error=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    _pxpIoReq *newSendReq=(_pxpIoReq *)pxprpc__malloc(sizeof(_pxpIoReq));
    newSendReq->self=self;
    newSendReq->buf.send=buf;
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

static void _pxprpcPipeConnectionClientReceive(struct pxprpc_abstract_io *self,struct pxprpc_bytes *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)((char *)self-(offsetof(_pxprpcPipeConnection,clientSide)));
    conn->clientSide.receive_error=NULL;
    if(conn->clientRecv.onCompleted!=NULL){
        conn->clientSide.receive_error=_pxpErrorOverlapedReceive;
        onCompleted(p);
        return;
    }
    if(conn->clientClosed || conn->serverClosed){
        conn->clientSide.receive_error=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    conn->clientRecv.self=self;
    conn->clientRecv.buf.recv=buf;
    conn->clientRecv.onCompleted=onCompleted;
    conn->clientRecv.p=p;
    _pxprpcPipePump(conn);
}


static void _pxprpcPipeConnectionServerSend(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)((char *)self-(offsetof(_pxprpcPipeConnection,serverSide)));
    conn->serverSide.send_error=NULL;
    if(conn->clientClosed || conn->serverClosed){
        conn->serverSide.send_error=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    _pxpIoReq *newSendReq=(_pxpIoReq *)pxprpc__malloc(sizeof(_pxpIoReq));
    newSendReq->self=self;
    newSendReq->buf.send=buf;
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

static void _pxprpcPipeConnectionServerRecv(struct pxprpc_abstract_io *self,struct pxprpc_bytes *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)((char *)self-(offsetof(_pxprpcPipeConnection,serverSide)));
    conn->serverSide.receive_error=NULL;
    if(conn->serverRecv.onCompleted!=NULL){
        conn->serverSide.receive_error=_pxpErrorOverlapedReceive;
        onCompleted(p);
        return;
    }
    if(conn->clientClosed || conn->serverClosed){
        conn->serverSide.receive_error=pxprpc_pipe_error_connection_closed;
        onCompleted(p);
        return;
    }
    conn->serverRecv.self=self;
    conn->serverRecv.buf.recv=buf;
    conn->serverRecv.onCompleted=onCompleted;
    conn->serverRecv.p=p;
    _pxprpcPipePump(conn);
}

static void _pxprpcPipeConnectionClosing(_pxprpcPipeConnection *conn){
    void (*onCompleted)(void *p);
    conn->serverSide.receive_error=pxprpc_pipe_error_connection_closed;
    conn->serverSide.send_error=pxprpc_pipe_error_connection_closed;
    conn->clientSide.receive_error=pxprpc_pipe_error_connection_closed;
    conn->clientSide.send_error=pxprpc_pipe_error_connection_closed;
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
    if(conn->clientClosed && conn->serverClosed){
        pxprpc__free(conn);
    }
}

static void _pxprpcPipeConnectionServerClose(struct pxprpc_abstract_io *self){
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)((char *)self-(offsetof(_pxprpcPipeConnection,serverSide)));
    conn->serverClosed=1;
    _pxprpcPipeConnectionClosing(conn);
}

static void _pxprpcPipeConnectionClientClose(struct pxprpc_abstract_io *self){
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)((char *)self-(offsetof(_pxprpcPipeConnection,clientSide)));
    conn->clientClosed=1;
    _pxprpcPipeConnectionClosing(conn);
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
    if(serv==NULL && on_connect!=NULL){
        serv=(_pxprpcPipeServer *)pxprpc__malloc(sizeof(_pxprpcPipeServer));
        serv->name=name;
        serv->on_connect=on_connect;
        serv->p=p;
        serv->next=_pxprpcServerList;
        _pxprpcServerList=serv;
    }
}

void (*pxprpc_pipe_executor)(void (*fn)(void *p),void *p)=NULL;

struct pxprpc_abstract_io *pxprpc_pipe_connect(const char *name){
    _pxprpcPipeServer *serv=_pxprpcServerList;
    while(serv!=NULL){
        if(strcmp(name,serv->name)==0){            
            break;
        }
        serv=serv->next;
    }
    if(serv==NULL){
        return NULL;
    }
    _pxprpcPipeConnection *conn=(_pxprpcPipeConnection *)pxprpc__malloc(sizeof(_pxprpcPipeConnection));
    conn->clientRecv.onCompleted=NULL;
    conn->clientClosed=0;
    conn->clientSide.send_error=NULL;
    conn->clientSide.receive_error=NULL;
    conn->clientSend=NULL;
    conn->clientSide.buf_free=&pxprpc__free;
    conn->clientSide.send=&_pxprpcPipeConnectionClientSend;
    conn->clientSide.receive=&_pxprpcPipeConnectionClientReceive;
    conn->clientSide.close=&_pxprpcPipeConnectionClientClose;

    conn->serverRecv.onCompleted=NULL;
    conn->serverClosed=0;
    conn->serverSide.send_error=NULL;
    conn->serverSide.receive_error=NULL;
    conn->serverSend=NULL;
    conn->serverSide.buf_free=&pxprpc__free;
    conn->serverSide.send=&_pxprpcPipeConnectionServerSend;
    conn->serverSide.receive=&_pxprpcPipeConnectionServerRecv;
    conn->serverSide.close=&_pxprpcPipeConnectionServerClose;
    serv->on_connect(&conn->serverSide,serv->p);
    return &conn->clientSide;
}
