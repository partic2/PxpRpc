
#include <pxprpc_pipe.h>
#include <stddef.h>
#include <stdint.h>

typedef struct _s_pxprpcPipeServer{
    const char *name;
    void (*on_connect)(struct pxprpc_abstract_io *);
    struct _s_pxprpcPipeServer *next;
} _pxprpcPipeServer;

static _pxprpcPipeServer *_pxprpcServerList=NULL;

#define _pxprpcConnection_state_idle 1
#define _pxprpcConnection_state_clientAccuired 2
#define _pxprpcConnection_state_serverAccuired 1

typedef struct _s_pxprpcPipeConnection{
    int state;
    struct pxprpc_buffer_part *clientRecvBuffer;
    struct pxprpc_buffer_part *servRecvBuffer;
    struct pxprpc_abstract_io clientSide;
    struct pxprpc_abstract_io serverSide;
    void (*clientSendOnCompleted)(void *self);
    void *clientSendOnCompletedP;
    void (*clientReceiveOnCompleted)(void *self);
    void *clientReceiveOnCompletedP;
    void (*serverSendOnCompleted)(void *self);
    void *serverSendOnCompletedP;
    void (*serverReceiveOnCompleted)(void *self);
    void *serverReceiveOnCompletedP;
} _pxprpcPipeConnection;


static void _pxprpcPipeConnectionClientSend(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){
    _pxprpcPipeConnection *conn=(void *)self-(offsetof(_pxprpcPipeConnection,clientSide));
    conn->clientSendOnCompleted=onCompleted;
    conn->clientSendOnCompletedP=p;
    struct pxprpc_buffer_part *consumerBuffer=conn->servRecvBuffer;
    struct pxprpc_buffer_part *tBuff=NULL;
    struct pxprpc_buffer_part *providerBuffer=buf;
    int providerOffset=0;
    int consumerOffset=0;
    while(consumerBuffer!=NULL){
        if(consumerBuffer->bytes.length==0){
            int remain=providerBuffer->bytes.length-providerOffset;
            tBuff=providerBuffer->next_part;
            while(tBuff!=NULL){
                remain+=tBuff->bytes.length;
                tBuff->next_part=tBuff;
            }
            consumerBuffer->bytes.base=pxprpc__malloc(remain);
            consumerBuffer->bytes.length=remain;
            while(providerBuffer!=NULL){
                int moveSize=providerBuffer->bytes.length-providerOffset;
                memmove(consumerBuffer->bytes.base+consumerOffset,providerBuffer->bytes.base+providerOffset,moveSize);
                providerBuffer=providerBuffer->next_part;
                consumerBuffer+=moveSize;
                providerOffset=0;
            }
        }else{
            while(providerBuffer!=NULL&&consumerOffset<consumerBuffer->bytes.length){
                int providerRemain=providerBuffer->bytes.length-providerOffset;
                int consumerRemain=consumerBuffer->bytes.length-consumerOffset;
                int moveSize=providerRemain>consumerRemain?consumerRemain:providerRemain;
                memmove(consumerBuffer->bytes.base+consumerOffset,providerBuffer->bytes.base+providerOffset,moveSize);
                if(providerOffset==providerBuffer->bytes.length){
                    providerBuffer=providerBuffer->next_part;
                    providerOffset=0;
                }
                consumerOffset+=moveSize;
            }
        }
        if(providerBuffer==NULL){
            break;
        }
    }
}

static void _pxprpcPipeConnectionClientReceive(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){

}

static void _pxprpcPipeConnectionServerSend(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){

}

static void _pxprpcPipeConnectionServerReceive(struct pxprpc_abstract_io *self,struct pxprpc_buffer_part *buf,void (*onCompleted)(void *args),void *p){

}

void pxprpc_pipe_serve(const char *name,void (*on_connect)(struct pxprpc_abstract_io *)){
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
}

void pxprpc_pipe_close(struct pxprpc_abstract_io *io){
}

