

#include "server_tbox.h"


#include <pxprpc/server.h>
#include <pxprpc/def.h>

struct _pxprpc_tbox{
    tb_socket_ref_t sock;
    struct pxprpc_namedfunc *namedFunc;
    int lenOfNamedFunc;
    int status;
    struct _pxprpc_tbox_sockconn *acceptedConn;
};

struct pxprpc_server_api *servapi;

int inited=0;

#define status_SUSPEND 1
#define status_RUNNING 2

static const char *errormsg=NULL;

static pxprpc_server_tbox pxprpc_new_tboxsocket(tb_socket_ref_t sock,struct pxprpc_namedfunc *namedFunc,int lenOfNamedFunc){
    if(inited==0){
        inited=1;
        pxprpc_server_query_interface(&servapi);
    }
    struct _pxprpc_tbox *self=(struct _pxprpc_tbox *)pxprpc__malloc(sizeof(struct _pxprpc_tbox));
    self->sock=sock;
    self->lenOfNamedFunc=lenOfNamedFunc;
    self->namedFunc=namedFunc;
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
};


static void __sockAbsIo1Read(struct pxprpc_abstract_io *self1,uint32_t length,uint8_t *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_tbox_sockconn *self=(struct _pxprpc_tbox_sockconn *)self1->userData;
    if(tb_socket_brecv(self->sock,buf,length)){
        self->readError=NULL;
    }else{
        self->readError="socket receive failed";
    }
    self->nextFn=onCompleted;
    self->nextFnArg0=p;
}
static void __sockAbsIo1Write(struct pxprpc_abstract_io *self1,uint32_t length,const uint8_t *buf){
    struct _pxprpc_tbox_sockconn *self=(struct _pxprpc_tbox_sockconn *)self1->userData;
    if(tb_socket_bsend(self->sock,buf,length)){
        self->writeError=NULL;
    }else{
        self->writeError="socket send failed";
    }
}
static const char *__sockAbsIo1GetError(struct pxprpc_abstract_io *self1,void *fn){
    struct _pxprpc_tbox_sockconn *self=(struct _pxprpc_tbox_sockconn *)self1->userData;
    if(fn==self1->read){
        return self->readError;
    }else if(fn==self1->write){
        return self->writeError;
    }
}

static struct _pxprpc_tbox_sockconn * __buildTboxSockconn(tb_socket_ref_t sock,struct _pxprpc_tbox *sCtx){
    struct _pxprpc_tbox_sockconn *sockconn=pxprpc__malloc(sizeof(struct _pxprpc_tbox_sockconn));
    sockconn->io1.userData=sockconn;
    sockconn->io1.read=&__sockAbsIo1Read;
    sockconn->io1.write=&__sockAbsIo1Write;
    sockconn->io1.get_error=&__sockAbsIo1GetError;
    sockconn->nextFn=NULL;
    sockconn->nextFnArg0=NULL;
    sockconn->sock=sock;
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
    sockconn->serv=sCtx;
    return sockconn;
}

static void __freeTboxSockconn(struct _pxprpc_tbox_sockconn *sc){
    servapi->context_delete(&sc->rpcCtx);
    tb_socket_exit(sc->sock);
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

static tb_int_t __serveTboxSockconn(tb_cpointer_t arg0){
    struct _pxprpc_tbox_sockconn *s1=(struct _pxprpc_tbox_sockconn *)arg0;
    servapi->context_start(s1->rpcCtx);
    while(s1->nextFn!=NULL){
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



