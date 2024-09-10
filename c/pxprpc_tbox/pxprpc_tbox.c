

#include <pxprpc_tbox.h>


#include <pxprpc.h>

struct _pxprpc_tbox{
    tb_socket_ref_t sock;
    pxprpc_funcmap *funcmap;
    int status;
    struct _pxprpc_tbox_sockconn *acceptedConn;
};

struct pxprpc_server_api *servapi;

int inited=0;

#define status_SUSPEND 1
#define status_RUNNING 2

static const char *errormsg=NULL;

static pxprpc_server_tbox pxprpc_new_tboxsocket(tb_socket_ref_t sock,pxprpc_funcmap *funcmap){
    if(inited==0){
        inited=1;
        pxprpc_server_query_interface(&servapi);
    }
    struct _pxprpc_tbox *self=(struct _pxprpc_tbox *)pxprpc__malloc(sizeof(struct _pxprpc_tbox));
    self->sock=sock;
    self->funcmap=funcmap;
    self->acceptedConn=NULL;
    return self;
}

struct _pxprpc_tbox_sockconn{
    struct _pxprpc_tbox *serv;
    struct _pxprpc_tbox_sockconn *prev;
    struct _pxprpc_tbox_sockconn *next;
    struct pxprpc_abstract_io io1;
    tb_socket_ref_t sock;
    pxprpc_server_context rpcCtx;
    void (*nextFn)(void *);
    void *nextFnArg0;
    const char* readError;
    const char* writeError;
    char running;
};


static void __sockAbsIo1Receive(struct pxprpc_abstract_io *self1,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_tbox_sockconn *self=(void *)self1-offsetof(struct _pxprpc_tbox_sockconn,io1);
    uint32_t packetRemain=0;
    self->readError=NULL;
    if(self->readError==NULL){
        if(!tb_socket_brecv(self->sock,(void *)&packetRemain,4)){
            self->readError="socket receive failed";
        }
    }
    struct pxprpc_buffer_part *curbuf=buf;
    while(curbuf!=NULL && self->readError==NULL){
        if(curbuf->next_part==NULL){
            /* last part, fill remain data */
            break;
        }
        if(curbuf->bytes.length==0)continue;
        if(!tb_socket_brecv(self->sock,curbuf->bytes.base,curbuf->bytes.length)){
            self->readError="tbox socket receive failed";
        }
        packetRemain-=curbuf->bytes.length;
        curbuf=curbuf->next_part;
    }
    if(curbuf!=NULL&&self->readError==NULL&&packetRemain>0){
        curbuf->bytes.length=packetRemain;
        curbuf->bytes.base=pxprpc__malloc(packetRemain);
        if(!tb_socket_brecv(self->sock,curbuf->bytes.base,curbuf->bytes.length)){
            self->readError="tbox socket receive failed";
        }
    }
    self->nextFn=onCompleted;
    self->nextFnArg0=p;
}
static void __sockAbsIo1Send(struct pxprpc_abstract_io *self1,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_tbox_sockconn *self=(void *)self1-offsetof(struct _pxprpc_tbox_sockconn,io1);
    uint32_t packetSize=0;
    struct pxprpc_buffer_part *curbuf=buf;
    self->writeError=NULL;
    while(curbuf!=NULL){
        packetSize+=curbuf->bytes.length;
        curbuf=curbuf->next_part;
    }
    if(!tb_socket_bsend(self->sock,(void *)&packetSize,4)){
        self->writeError="tbox socket send failed";
    }
    curbuf=buf;
    while(curbuf!=NULL && self->writeError==NULL){
        if(!tb_socket_bsend(self->sock,curbuf->bytes.base,curbuf->bytes.length)){
            self->writeError="tbox socket send failed";
        }
        curbuf=curbuf->next_part;
    }
    self->nextFn=onCompleted;
    self->nextFnArg0=p;
}
static const char *__sockAbsIo1GetError(struct pxprpc_abstract_io *self1,void *fn){
    struct _pxprpc_tbox_sockconn *self=(void *)self1-offsetof(struct _pxprpc_tbox_sockconn,io1);
    if(fn==self1->receive){
        return self->readError;
    }else if(fn==self1->send){
        return self->writeError;
    }
}

static const void __sockAbsIo1Close(struct pxprpc_abstract_io *self1){
    struct _pxprpc_tbox_sockconn *self=(void *)self1-offsetof(struct _pxprpc_tbox_sockconn,io1);
    tb_socket_exit(self->sock);
}

static void __stopServeSockconn(struct _pxprpc_tbox_sockconn *sc){
    sc->running=0;
}

static struct _pxprpc_tbox_sockconn * __buildTboxSockconn(tb_socket_ref_t sock,struct _pxprpc_tbox *sCtx){
    struct _pxprpc_tbox_sockconn *sockconn=pxprpc__malloc(sizeof(struct _pxprpc_tbox_sockconn));
    sockconn->io1.send=&__sockAbsIo1Send;
    sockconn->io1.receive=&__sockAbsIo1Receive;
    sockconn->io1.get_error=&__sockAbsIo1GetError;
    sockconn->io1.buf_free=&pxprpc__free;
    sockconn->io1.close=&__sockAbsIo1Close;
    sockconn->nextFn=NULL;
    sockconn->nextFnArg0=NULL;
    sockconn->sock=sock;
    sockconn->readError=NULL;
    sockconn->writeError=NULL;
    sockconn->running=0;
    servapi->context_new(&sockconn->rpcCtx,&sockconn->io1);
    struct pxprpc_server_context_exports *ctxexp=servapi->context_exports(sockconn->rpcCtx);
    ctxexp->funcmap=sCtx->funcmap;
    ctxexp->on_closed=(void(*)(void *cb_data))__stopServeSockconn;
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
    sockconn->serv=sCtx;
    return sockconn;
}

static void __freeTboxSockconn(struct _pxprpc_tbox_sockconn *sc){
    if(sc->next!=NULL){
        sc->next->prev=sc->prev;
    }
    if(sc->prev!=NULL){
        sc->prev->next=sc->next;
    }
    if(sc->serv->acceptedConn==sc){
        sc->serv->acceptedConn=sc->next;
    }
    servapi->context_delete(&sc->rpcCtx);
    pxprpc__free(sc);
}

static tb_int_t __serveTboxSockconn(tb_cpointer_t arg0){
    struct _pxprpc_tbox_sockconn *s1=(struct _pxprpc_tbox_sockconn *)arg0;
    servapi->context_start(s1->rpcCtx);
    s1->running=1;
    while(s1->running && s1->nextFn!=NULL){
        void (*fn)(void *);
        fn=s1->nextFn;
        s1->nextFn=NULL;
        fn(s1->nextFnArg0);
    }
    __freeTboxSockconn(s1);
}

static void pxprpc_serve_block(pxprpc_server_tbox serv){
    struct _pxprpc_tbox *self=(struct _pxprpc_tbox *)serv;
    self->status|=status_RUNNING;
    if(tb_socket_listen(self->sock,8)==tb_false){
        errormsg="tb_socket_listen failed!";
        return;
    }
    tb_ipaddr_t remoteAddr;
    while(self->status&status_RUNNING){
        if(tb_socket_wait(self->sock,TB_SOCKET_EVENT_ACPT,-1)<0){
            errormsg="tb_socket_wait failed!";
            return;
        }
        tb_socket_ref_t connSock=tb_socket_accept(self->sock,&remoteAddr);
        if(connSock==NULL){
            errormsg="tb_socket_accept failed!";
            break;
        }
        struct _pxprpc_tbox_sockconn *sc=__buildTboxSockconn(connSock,self);
        tb_thread_init("thread-pxprpc-servconn",__serveTboxSockconn,sc,0);
    }
}

static int pxprpc_server_delete(pxprpc_server_tbox serv){
    struct _pxprpc_tbox *self=(struct _pxprpc_tbox *)serv;
    struct _pxprpc_tbox_sockconn *conn2=NULL;
    for(struct _pxprpc_tbox_sockconn *conn=self->acceptedConn;conn!=NULL;conn=conn2){
        conn2=conn->next;
        __freeTboxSockconn(conn);
    }
    pxprpc__free(serv);
    return 0;
}


static const char *pxprpc_tbox_get_error(){
    return errormsg;
}

static void pxprpc_tbox_clean_error(){
    errormsg=NULL;
}

static pxprpc_tbox_api exports={
    &pxprpc_new_tboxsocket,&pxprpc_serve_block,&pxprpc_server_delete,&pxprpc_tbox_get_error,&pxprpc_tbox_clean_error
};

extern int pxprpc_tbox_query_interface(pxprpc_tbox_api **outapi){
    *outapi=&exports;
}



