

#include "server_tbox.h"

#include <tbox/tbox.h>

#include "../pxprpc/server.h"
#include "../pxprpc/def.h"

struct _pxprpc_tbox{
    tb_socket_ref_t sock;
    struct pxprpc_namedfunc *namedFunc;
    int lenOfNamedFunc;
    int status;
};

struct pxprpc_server_api servapi;

int inited=0;

#define status_SUSPEND 1
#define status_RUNNING 2


extern pxprpc_server_tbox pxprpc_new_tboxsocket(tb_socket_ref_t sock,const struct pxprpc_namedfunc *namedFunc,int lenOfNamedFunc){
    if(inited==0){
        inited=1;
        pxprpc_query_interface(&servapi,sizeof(struct pxprpc_server_api));
    }
    struct _pxprpc_tbox *self=(struct _pxprpc_tbox *)pxprpc__malloc(sizeof(struct _pxprpc_tbox));
    self->sock=sock;
    self->lenOfNamedFunc=lenOfNamedFunc;
    self->namedFunc=namedFunc;
    
    return self;
}

struct _pxprpc_tbox_sockconn{
    struct pxprpc_abstract_io io1;
    tb_socket_ref_t sock;
    pxprpc_server_context rpcCtx;
    void (*nextFn)(void *);
    void *nextFnArg0;
};

static void __sockAbsIo1Read(struct pxprpc_abstract_io *self1,uint32_t length,uint8_t *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_tbox_sockconn *self=(struct _pxprpc_tbox_sockconn *)self1->reserved;
    if(tb_socket_brecv(self->sock,buf,length)==tb_true){
        self->nextFn=onCompleted;
        self->nextFnArg0=p;
    }
    
}
static void __sockAbsIo1Write(struct pxprpc_abstract_io *self1,uint32_t length,uint8_t *buf,void (*onCompleted)(void *p),void *p){
    struct _pxprpc_tbox_sockconn *self=(struct _pxprpc_tbox_sockconn *)self1->reserved;
    tb_socket_bsend(self->sock,buf,length); 
}

static struct _pxprpc_tbox_sockconn * __buildTboxSockconn(tb_socket_ref_t sock,struct _pxprpc_tbox *sCtx){
    struct _pxprpc_tbox_sockconn *sockconn=pxprpc__malloc(sizeof(struct _pxprpc_tbox_sockconn));
    sockconn->io1.reserved=&sockconn;
    sockconn->io1.read=&__sockAbsIo1Read;
    sockconn->io1.write=&__sockAbsIo1Write;
    sockconn->nextFn=NULL;
    sockconn->nextFnArg0=NULL;
    sockconn->sock=sock;
    servapi.context_new(&sockconn->rpcCtx,&sockconn->io1,sCtx->namedFunc,sCtx->lenOfNamedFunc);
    return sockconn;
}

static void __freeTboxSockconn(struct _pxprpc_tbox_sockconn *sc){
    servapi.context_delete(sc->rpcCtx);
    pxprpc__free(sc);
}

static void __serveTboxSockconn(struct _pxprpc_tbox_sockconn *s1){
    servapi.context_start(s1->rpcCtx);
    while(s1->nextFn!=NULL){
        void (*fn)(void *);
        fn=s1->nextFn;
        s1->nextFn=NULL;
        fn(s1->nextFnArg0);
    }
    __freeTboxSockconn(s1);
}

extern void *pxprpc_serve_block(pxprpc_server_tbox serv){
    struct _pxprpc_tbox *self=(struct _pxprpc_tbox *)serv;
    self->status|=status_RUNNING;
    tb_socket_listen(self->sock,8);
    tb_ipaddr_ref_t remoteAddr;
    while(self->status&status_RUNNING){
        tb_socket_ref_t connSock=tb_socket_accept(self->sock,&remoteAddr);
        if(connSock==NULL){
            break;
        }
        struct _pxprpc_tbox_sockconn *sc=__buildTboxSockconn(connSock,self);
        tb_thread_init("thread-pxprpc-servconn",__serveTboxSockconn,sc,0);
    }
}



