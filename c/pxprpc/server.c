#ifndef _PXPRPC_SERVER_C
#define _PXPRPC_SERVER_C

#include "utils.c"

#include <pxprpc.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>


#define MACRO_TO_STR(x) #x
#define MACRO_TO_STR2(s) MACRO_TO_STR(s)


static const char *ServerInfo="server name:pxprpc for c\n"
"version:2.0\n";

static struct pxprpc_server_context_exports *pxprpc_server_context_exports(pxprpc_server_context context);


struct _pxprpc__ServCo{
    struct pxprpc_server_context_exports exp;
    uint32_t status;
    uint32_t sequenceSessionMask;
    char sequenceMaskBitsCnt;
    struct pxprpc_bytes recvBuf;
    /* context should only free when refCount=0. Pending request will hold the reference.*/
    int32_t refCount;
    pxprpc_request *pendingReqTail;
};

#define _pxprpc__ServCoIsClosed(self) (self->status&0x1)


//Will free memory of  server_context when refCount reach 0;
static void pxprpc_refCount_add(pxprpc_server_context server_context,int32_t addend){
    struct _pxprpc__ServCo *self=server_context;
    self->refCount+=addend;
    if(self->refCount==0){
        pxprpc__free(server_context);
    }
}

static int pxprpc_closed(void *server_context){
    return _pxprpc__ServCoIsClosed(((struct _pxprpc__ServCo*)server_context));
}

static int pxprpc_close(pxprpc_server_context server_context){
    if(pxprpc_closed(server_context)){
        return 0;
    }
    struct _pxprpc__ServCo *self=server_context;
    for(int i=0;i<self->exp.ref_pool_size;i++){
        pxprpc_ref *ref=&self->exp.ref_pool[i];
        if(ref->on_free!=NULL){
            ref->on_free(&self->exp.ref_pool[i]);
            ref->on_free=NULL;
        }
        ref->object=NULL;
    }
    self->status|=0x1;
    self->exp.io->close(self->exp.io);
    if(self->exp.on_closed!=NULL){
        self->exp.on_closed(self->exp.cb_data);
    }
    return 0;
}


static void _pxprpc__step1(struct _pxprpc__ServCo *self);

static void _pxprpc__ExpandRefPool(struct _pxprpc__ServCo *self){
    int start=self->exp.ref_pool_size;
    int end=self->exp.ref_pool_size+REF_POOL_EXPAND_COUNT-1;
    if(self->exp.ref_pool==NULL){
        self->exp.ref_pool=pxprpc__malloc(sizeof(pxprpc_ref)*(end+1));
    }else{
        self->exp.ref_pool=pxprpc__realloc(self->exp.ref_pool,sizeof(pxprpc_ref)*(end+1));
    }
    self->exp.ref_pool_size=end+1;
    pxprpc_ref *refPool=self->exp.ref_pool;
    for(int i=start;i<end;i++){
        refPool[i].nextFree=&refPool[i+1];
    }
    refPool[end].nextFree=NULL;
    for(int i=start;i<=end;i++){
        refPool[i].on_free=NULL;
        refPool[i].object=NULL;
    }
    refPool[end].nextFree=self->exp.free_ref_entry;
    self->exp.free_ref_entry=&refPool[start];
}

static pxprpc_ref *pxprpc_alloc_ref(pxprpc_server_context server_context,uint32_t *index){
    struct _pxprpc__ServCo *self=server_context;
    pxprpc_ref *freeref=self->exp.free_ref_entry;
    if(freeref==NULL){
        _pxprpc__ExpandRefPool(self);
    }
    freeref=self->exp.free_ref_entry;
    self->exp.free_ref_entry=freeref->nextFree;
    *index=freeref-self->exp.ref_pool;
    return freeref;
}

static void pxprpc_free_ref(pxprpc_server_context server_context,pxprpc_ref *ref2){
    struct _pxprpc__ServCo *self=server_context;
    if(ref2->on_free!=NULL){
        ref2->on_free(ref2);
        ref2->on_free=NULL;
    }
    ref2->object=NULL;
    ref2->nextFree=self->exp.free_ref_entry;
    self->exp.free_ref_entry=ref2;
}

static pxprpc_ref *pxprpc_get_ref(pxprpc_server_context server_context,uint32_t index){
    struct _pxprpc__ServCo *self=server_context;
    return &self->exp.ref_pool[index];
}

static void _pxprpc__ServCoStart(struct _pxprpc__ServCo *self){
    self->exp.ref_pool_size=0;
    self->exp.ref_pool=NULL;
    self->pendingReqTail=NULL;
    self->sequenceSessionMask=0xffffffff;
    self->status=0;
    _pxprpc__step1(self);
}


static void _pxprpc__queueRequest(pxprpc_request *r){
    struct _pxprpc__ServCo *self=r->server_context;
    if(self->sequenceSessionMask==0xffffffff || self->sequenceSessionMask!=(r->session>>(32-self->sequenceMaskBitsCnt)) ){
        r->inSequence=0;
        r->next_step(r);
    }else{
        r->inSequence=0;
        for(pxprpc_request *r2=self->pendingReqTail;r2!=NULL;r2=r2->lastReq){
            if((r2->session) == (r->session)){
                r->lastReq=r2;
                r->nextReq=r2->nextReq;
                if(r->nextReq!=NULL){
                    r->nextReq->lastReq=r;
                }
                r2->nextReq=r;
                r->inSequence=1;
                break;
            }
        }
        if(r->inSequence==0){
            if(self->pendingReqTail!=NULL){
                self->pendingReqTail->nextReq=r;
            }
            r->lastReq=self->pendingReqTail;
            r->inSequence=1;
            r->next_step(r);
        }
    }
}

static void _pxprpc__finishRequest(pxprpc_request *r){
    struct _pxprpc__ServCo * ctx=(struct _pxprpc__ServCo *)r->server_context;
    if(ctx->exp.io->send_error!=NULL){
        ctx->exp.last_error=ctx->exp.io->send_error;
        pxprpc_close(ctx);
        return;
        
    }
    if(r->on_finish!=NULL)r->on_finish(r);
    if(r->parameter.base!=NULL){
        ctx->exp.io->buf_free((uint8_t *)(r->parameter.base)-8);
    }
    if(r->inSequence){
        if(r->lastReq!=NULL){
            r->lastReq->nextReq=r->nextReq;
        }
        if(r->nextReq!=NULL){
            r->nextReq->lastReq=r->lastReq;
            if((r->nextReq->session)==(r->session)){
                r->nextReq->next_step(r->nextReq);
            }
        }
    }
    pxprpc_refCount_add(r->server_context,-1);
    pxprpc__free(r);
}

static pxprpc_request *_pxprpc__newRequest(pxprpc_server_context context,uint32_t session){
    pxprpc_request *r=pxprpc__malloc(sizeof(pxprpc_request));
    memset(r,0,sizeof(pxprpc_request));
    r->session=session;
    r->server_context=context;
    r->nextReq=NULL;
    r->lastReq=NULL;
    pxprpc_refCount_add(r->server_context,1);
    return r;
}


//Call Handler

static void _pxprpc__stepCall2(pxprpc_request *r){
    struct _pxprpc__ServCo * ctx=(struct _pxprpc__ServCo *)r->server_context;
    r->sendBuf.bytes.base=&r->session;
    if(r->rejected){r->session=r->session^0x80000000;}
    r->sendBuf.bytes.length=4;
    if(r->result.bytes.length){
        r->sendBuf.next_part=&r->result;
    }else{
        r->sendBuf.next_part=NULL;
    }
    r->next_step=_pxprpc__finishRequest;
    if(!_pxprpc__ServCoIsClosed(ctx)){
        ctx->exp.io->send(ctx->exp.io,&r->sendBuf,(void *)r->next_step,r);
    }
}
static void _pxprpc__stepCall1(pxprpc_request *r){
    struct _pxprpc__ServCo * ctx=(struct _pxprpc__ServCo *)r->server_context;
    pxprpc_callable *callable=ctx->exp.ref_pool[r->callable_index].object;
    r->next_step=_pxprpc__stepCall2;
    callable->call(callable,r);
}

static void _pxprpc__stepFreeRef1(pxprpc_request *r){
    for(int i=0;i<r->parameter.length;i+=4){
        pxprpc_free_ref(r->server_context,pxprpc_get_ref(r->server_context,*(uint32_t *)(r->parameter.base+i)));
    }
    _pxprpc__stepCall2(r);
}

//GetFunc handler
static void _pxprpc__stepGetFunc1(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    r->temp1=-1;
    int lenOut=-1;
    if(self->exp.funcmap!=NULL){
        pxprpc_callable *func=self->exp.funcmap->get(self->exp.funcmap,r->parameter.base,r->parameter.length);
        if(func!=NULL){
            pxprpc_ref *ref2=pxprpc_alloc_ref(self,(uint32_t *)&r->temp1);
            ref2->object=func;
        }
    }
    r->result.bytes.base=&r->temp1;
    r->result.bytes.length=4;
    _pxprpc__stepCall2(r);
}


static void _pxprpc__stepClose1(pxprpc_request *r){
    pxprpc_close(r->server_context);
}


//GetInfo handler
static void _pxprpc__stepGetInfo1(pxprpc_request *r){
    r->result.bytes.base=(void *)ServerInfo;
    r->result.bytes.length=strlen(ServerInfo);
    _pxprpc__stepCall2(r);
}


static void _pxprpc__stepSequence1(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    uint32_t sessionMask=0xffffffff;
    memmove(&sessionMask,r->parameter.base,4);
    if(sessionMask==0xffffffff){
        self->sequenceSessionMask=0xffffffff;
        pxprpc_request *req;
        pxprpc_request *req2;
        for(req=self->pendingReqTail;req!=NULL;req=req2){
            req2=req->lastReq;
            /* discard all queued request */
            if(req2!=NULL && (req2->session)==(req->session)){
                pxprpc__free(req);
            }else{
                req->nextReq=NULL;
                req->lastReq=NULL;
            }
        }
        self->pendingReqTail=NULL;
    }else{
        self->sequenceMaskBitsCnt=sessionMask&0xff;
        self->sequenceSessionMask=sessionMask>>(32-self->sequenceMaskBitsCnt);
    }
    _pxprpc__stepCall2(r);
}


static void _pxprpc__step2(struct _pxprpc__ServCo *self){
    if(_pxprpc__ServCoIsClosed(self)){
        return;
    }
    self->exp.last_error=self->exp.io->receive_error;
    if(self->exp.last_error!=NULL){
        pxprpc_close(self);
        return;
    }
    pxprpc_request *req=_pxprpc__newRequest(self,*((uint32_t *)(self->recvBuf.base)));
    req->callable_index=*((uint32_t *)(self->recvBuf.base)+1);
    req->parameter.base=(void *)((uint8_t *)(self->recvBuf.base)+8);
    req->parameter.length=self->recvBuf.length-8;
    
    if(req->callable_index>=0){
        req->next_step=_pxprpc__stepCall1;
    }else{
        switch(-req->callable_index){
            case 1:
            req->next_step=_pxprpc__stepGetFunc1;
            break;
            case 2:
            req->next_step=_pxprpc__stepFreeRef1;
            break;
            case 3:
            req->next_step=_pxprpc__stepClose1;
            break;
            case 4:
            req->next_step=_pxprpc__stepGetInfo1;
            break;
            case 5:
            req->next_step=_pxprpc__stepSequence1;
            break;
            default:
            /* XXX: should close? */
            break;
        }
    }
    _pxprpc__queueRequest(req);
    _pxprpc__step1(self);
}

static void _pxprpc__step1(struct _pxprpc__ServCo *self){
    if(_pxprpc__ServCoIsClosed(self)){
        return;
    }
    self->exp.io->receive(self->exp.io,&self->recvBuf,(void *)_pxprpc__step2,self);
}



static int pxprpc_new_server_context(pxprpc_server_context *server_context,struct pxprpc_abstract_io *io1){
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)pxprpc__malloc(sizeof(struct _pxprpc__ServCo));
    memset(ctx,0,sizeof(struct _pxprpc__ServCo));
    ctx->exp.io=io1;
    *server_context=ctx;
    ctx->refCount=1;
    return 0;
}

static int pxprpc_start_serve(pxprpc_server_context server_context){
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)server_context;
    _pxprpc__ServCoStart(ctx);
    return 0;
}

static int pxprpc_free_context(pxprpc_server_context *server_context){
    if(*server_context==NULL)return 0;
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)*server_context;
    pxprpc_close(ctx);
    pxprpc_refCount_add(*server_context,-1);
    *server_context=NULL;
    return 0;
}

static struct pxprpc_server_context_exports *pxprpc_server_context_exports(pxprpc_server_context context){
    struct _pxprpc__ServCo *self=context;
    return &self->exp;
}


static void pxprpc_server_context_exports_apply(pxprpc_server_context context){
}


static pxprpc_server_api exports={
    &pxprpc_new_server_context,&pxprpc_start_serve,&pxprpc_closed,&pxprpc_close,&pxprpc_free_context,
    &pxprpc_alloc_ref,&pxprpc_free_ref,&pxprpc_get_ref,&pxprpc_server_context_exports,
    &pxprpc_server_context_exports_apply
};

extern const char *pxprpc_server_query_interface(pxprpc_server_api **outapi){
    *outapi=&exports;
    return NULL;
}


#endif