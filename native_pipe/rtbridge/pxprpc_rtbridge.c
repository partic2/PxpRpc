/* pxprpc runtime bridge is mainly about communicate between runtime in one process. */

#include <stdint.h>
#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <uv.h>

#include "pxprpc_rtbridge.h"

typedef struct pxprpc_rtbridge_event_s _rtbridgeEvent;

#define _rtbridgeEvent_id_pipe_accept 1
#define _rtbridgeEvent_id_pipe_connected 2
#define _rtbridgeEvent_id_pipe_receiveDone 3
#define _rtbridgeEvent_id_pipe_sendDone 4

#define _rtbridgeEvent_id_pipe_serve 0x11
#define _rtbridgeEvent_id_pipe_connect 0x12
#define _rtbridgeEvent_id_pipe_receive 0x13
#define _rtbridgeEvent_id_pipe_send 0x14
#define _rtbridgeEvent_id_pipe_close 0x15

struct _pxprpcRbCtx_s{
    _rtbridgeEvent *firstResult;
    _rtbridgeEvent *queuedResult;
    _rtbridgeEvent *firstInvoc;
    uv_mutex_t rmtx;
    uv_cond_t rsig;
    uv_mutex_t imtx;
    uv_async_t ihandler;
};

typedef struct _pxprpcRbCtx_s *rbctx;


static void _pxprpcCleanResultQueue(rbctx c){
    _rtbridgeEvent *ev=c->firstResult;
    while(c->firstResult!=NULL && c->firstResult->processed){
        ev=c->firstResult->next;
        pxprpc__free(c->firstResult);
        c->firstResult=ev;
    }
}


struct pxprpc_rtbridge_event_s *pxprpc_rtbridge_pull_result(pxprpc_rtbridge_context context){
    rbctx c=context;
    _rtbridgeEvent *ev=NULL;
    uv_mutex_lock(&c->rmtx);
    _pxprpcCleanResultQueue(c);
    while(c->queuedResult==NULL){
        uv_cond_wait(&c->rsig,&c->rmtx);
    }
    ev=c->queuedResult;
    c->queuedResult=ev->next;
    uv_mutex_unlock(&c->rmtx);
    return ev;
}

static void _pxprpcSendQueueResult(rbctx c,_rtbridgeEvent *evt){
    evt->context=c;
    uv_mutex_lock(&c->rmtx);
    if(c->firstResult==NULL){
        c->firstResult=evt;
    }else{
        _rtbridgeEvent *je=c->firstResult;
        while(je->next!=NULL){
            je=je->next;
        }
        je->next=evt;
    }
    if(c->queuedResult==NULL){
        c->queuedResult=evt;
    }
    uv_cond_signal(&c->rsig);
    uv_mutex_unlock(&c->rmtx);
}


void _pxprpcRtbridgeOnConnectHandler(struct pxprpc_abstract_io *conn,void *p){
    _rtbridgeEvent *evt=pxprpc__malloc(sizeof(_rtbridgeEvent));
    evt->eventSize=sizeof(_rtbridgeEvent);
    evt->id=_rtbridgeEvent_id_pipe_accept;
    evt->processed=0;
    evt->next=NULL;
    evt->p2.accept.connId=(uint64_t)conn;
    evt->p2.accept.p=(uint64_t)p;
    _pxprpcSendQueueResult(evt->context,evt);
}

void _pxprpcRtbridgeOnSendDoneHandler(void *p){
    _rtbridgeEvent *evt2=(_rtbridgeEvent *)p;
    _rtbridgeEvent *evt=pxprpc__malloc(sizeof(_rtbridgeEvent));
    evt->eventSize=sizeof(_rtbridgeEvent);
    evt->id=_rtbridgeEvent_id_pipe_sendDone;
    evt->processed=0;
    evt->next=NULL;
    evt->p2.sendDone.connId=evt->p2.send.connId;
    evt->p2.sendDone.p=evt->p2.send.p;
    _pxprpcSendQueueResult(evt2->context,evt);
    evt2->processed=1;
}

void _pxprpcRtbridgeOnReceiveDoneHandler(void *p){
    _rtbridgeEvent *evt2=(_rtbridgeEvent *)p;
    _rtbridgeEvent *evt=pxprpc__malloc(offsetof(_rtbridgeEvent,p3)+evt2->p2.receive.buf.bytes.length);
    evt->eventSize=offsetof(_rtbridgeEvent,p3)+evt2->p2.receive.buf.bytes.length;
    evt->id=_rtbridgeEvent_id_pipe_receiveDone;
    evt->processed=0;
    evt->next=NULL;
    evt->p2.receiveDone.connId=evt2->p2.receive.connId;
    evt->p2.receiveDone.len=evt2->p2.receive.buf.bytes.length;
    memmove(&evt->p3,evt2->p2.receive.buf.bytes.base,evt2->p2.receive.buf.bytes.length);
    _pxprpcSendQueueResult(evt2->context,evt);
    evt2->processed=1;
}


void _pxprpcRtbInvokeHandler(uv_async_t *handle){
    rbctx c=uv_handle_get_data((uv_handle_t *)handle);
    uv_mutex_lock(&c->imtx);
    while(c->firstInvoc!=NULL){
        switch(c->firstInvoc->id){
            case _rtbridgeEvent_id_pipe_serve:
            if(c->firstInvoc->p2.serve.startOrStop){
                pxprpc_pipe_serve(&c->firstInvoc->p3,_pxprpcRtbridgeOnConnectHandler,*(void **)&c->firstInvoc->p2.serve.p);
            }else{
                pxprpc_pipe_serve(&c->firstInvoc->p3,_pxprpcRtbridgeOnConnectHandler,*(void **)&c->firstInvoc->p2.serve.p);
            }
            c->firstInvoc->processed=1;
            c->firstInvoc=c->firstInvoc->next;
            break;
            case _rtbridgeEvent_id_pipe_connect:{
                struct pxprpc_abstract_io *connected=pxprpc_pipe_connect(&c->firstInvoc->p3);
                _rtbridgeEvent *evt=pxprpc__malloc(sizeof(_rtbridgeEvent));
                evt->eventSize=sizeof(_rtbridgeEvent);
                evt->id=_rtbridgeEvent_id_pipe_connected;
                evt->processed=0;
                evt->next=NULL;
                evt->p2.connected.connId=(uint64_t)connected;
                evt->p2.connected.p=c->firstInvoc->p2.connect.p;
                _pxprpcSendQueueResult(evt->context,evt);
                c->firstInvoc->processed=1;
                c->firstInvoc=c->firstInvoc->next;
            }
            break;
            case _rtbridgeEvent_id_pipe_send:{
                struct pxprpc_abstract_io *conn=(&c->firstInvoc->p2.send.connId);
                struct pxprpc_buffer_part *buf=&c->firstInvoc->p2.send.buf;
                buf->bytes.base=&c->firstInvoc->p3;
                buf->bytes.length=c->firstInvoc->p2.send.dataSize;
                buf->next_part=NULL;
                conn->send(conn,buf,_pxprpcRtbridgeOnSendDoneHandler,c->firstInvoc);
                c->firstInvoc=c->firstInvoc->next;
            }
            break;
            case _rtbridgeEvent_id_pipe_receive:{
                struct pxprpc_abstract_io *conn=(&c->firstInvoc->p2.receive.connId);
                struct pxprpc_buffer_part *buf=&c->firstInvoc->p2.receive.buf;
                buf->bytes.length=0;
                buf->next_part=NULL;
                conn->receive(conn,buf,_pxprpcRtbridgeOnReceiveDoneHandler,c->firstInvoc);
                c->firstInvoc=c->firstInvoc->next;
            }
            break;
            case _rtbridgeEvent_id_pipe_close:{
                struct pxprpc_abstract_io *conn=(&c->firstInvoc->p2.receive.connId);
                conn->close(conn);
                c->firstInvoc->processed=1;
                c->firstInvoc=c->firstInvoc->next;
            }
            break;
        }
    }
    uv_mutex_unlock(&c->imtx);
}

static uv_loop_t *thisloop=NULL;

void pxprpc_rtbridge_push_invoc(pxprpc_rtbridge_context context,struct pxprpc_rtbridge_event_s *evt){
    rbctx c=context;
    uv_mutex_lock(&c->imtx);
    if(c->firstInvoc==NULL){
        c->firstInvoc=evt;
    }else{
        _rtbridgeEvent *je=c->firstInvoc;
        while(je->next!=NULL){
            je=je->next;
        }
        je->next=evt;
    }
    uv_mutex_unlock(&c->imtx);
    uv_async_send(&c->ihandler);
}


pxprpc_rtbridge_context pxprpc_rtbridge_alloc(){
    /* accept other loop? */
    thisloop=uv_default_loop();
    rbctx c=pxprpc__malloc(sizeof(struct _pxprpcRbCtx_s));
    memset(c,0,sizeof(struct _pxprpcRbCtx_s));
    uv_mutex_init(&c->imtx);
    uv_mutex_init(&c->rmtx);
    uv_cond_init(&c->rsig);
    uv_async_init(thisloop,&c->ihandler,_pxprpcRtbInvokeHandler);
    uv_handle_set_data((uv_handle_t *)&c->ihandler,c);
    return c;
}

void _pxprpcRtbridgeCloseHandle(uv_handle_t* handle){
    rbctx c=uv_handle_get_data(handle);
    pxprpc__free(c);
}

void pxprpc_rtbridge_free(pxprpc_rtbridge_context context){
    rbctx c=pxprpc__malloc(sizeof(struct _pxprpcRbCtx_s));
    uv_mutex_destroy(&c->imtx);
    uv_mutex_destroy(&c->rmtx);
    uv_cond_destroy(&c->rsig);
    uv_close(&c->ihandler,_pxprpcRtbridgeCloseHandle);
}

