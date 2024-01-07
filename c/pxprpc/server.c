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
    struct pxprpc_abstract_io *io1;
    struct pxprpc_server_context_exports exp;
    int lengthOfNamedFuncs;
    struct pxprpc_namedfunc *namedfuncs;
    uint32_t status;
    uint32_t sequenceSessionMask;
    char sequenceMaskBitsCnt;
    #pragma pack(1)
    union{
        struct{
            uint32_t session;
            int32_t callableIndex;
        } hdr;
        uint8_t hdrbuf[8];
    };
    #pragma pack()
    struct pxprpc_buffer_part recvBuf[2];
    pxprpc_request *pendingReqTail;
};

#define _pxprpc__ServCoIsClosed(self) self->status&0x1

static int pxprpc_closed(void *server_context){
    return _pxprpc__ServCoIsClosed(((struct _pxprpc__ServCo*)server_context));
}

static int pxprpc_close(pxprpc_server_context server_context){
    if(pxprpc_closed(server_context)){
        return 0;
    }
    struct _pxprpc__ServCo *self=server_context;
    for(int i=0;i<self->exp.ref_pool_size;i++){
        if(self->exp.ref_pool[i].on_free!=NULL){
            self->exp.ref_pool[i].on_free(&self->exp.ref_pool[i]);
        }
    }
    self->status|=0x1;
    self->io1->close(self->io1);
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
    if(ref2->on_free!=NULL)ref2->on_free(ref2);
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
            if((r2->session&0xffffff00) == (r->session&0xffffff00)){
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
    if(r->on_finish!=NULL)r->on_finish(r);
    if(r->parameter.base!=NULL){
        ctx->io1->buf_free(r->parameter.base);
    }
    if(r->inSequence){
        if(r->lastReq!=NULL){
            r->lastReq->nextReq=r->nextReq;
        }
        if(r->nextReq!=NULL){
            r->nextReq->lastReq=r->lastReq;
            if((r->nextReq->session&0xffffff00)==(r->session&0xffffff00)){
                r->nextReq->next_step(r->nextReq);
            }
        }
    }
    pxprpc__free(r);
}

static pxprpc_request *_pxprpc__newRequest(pxprpc_server_context context,uint32_t session){
    pxprpc_request *r=pxprpc__malloc(sizeof(pxprpc_request));
    memset(r,0,sizeof(pxprpc_request));
    r->session=session;
    r->server_context=context;
    r->nextReq=NULL;
    r->lastReq=NULL;
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
    ctx->io1->send(ctx->io1,&r->sendBuf,(void *)r->next_step,r);
}
static void _pxprpc__stepCall1(pxprpc_request *r){
    struct _pxprpc__ServCo * ctx=(struct _pxprpc__ServCo *)r->server_context;
    pxprpc_callable *callable=ctx->exp.ref_pool[r->callable_index].object;
    r->next_step=_pxprpc__stepCall2;
    callable->call(callable,r);
}

static void _pxprpc__stepFreeRef1(pxprpc_request *r){
    pxprpc_free_ref(r->server_context,pxprpc_get_ref(r->server_context,*(uint32_t *)r->parameter.base));
    _pxprpc__stepCall2(r);
}

//str1(2)Len = -1 indicate 0-ending string, In this case the real length will be put into *str1(2)Len after function return.
//string length is limited to 0x1000
static int _pxprpc__strcmp2(const char *str1,int *str1Len,const char *str2,int *str2Len){
    int i,r;
    if(*str1Len==-1){
        for(i=0;i<0x1000;i++){
            if(str1[i]==0){
                break;
            }
        }
        *str1Len=i;
    }
    if(*str2Len==-1){
        for(i=0;i<0x1000;i++){
            if(str2[i]==0){
                break;
            }
        }
        *str2Len=i;
    }
    if(*str1Len==*str2Len){
        for(i=0,r=0;i<*str1Len;i++){
            if(str1[i]!=str2[i]){
                r=str1[i]-str2[i];
                break;
            }
        }
    }else{
        r=*str1Len-*str2Len;
    }
    return r;
}
//GetFunc handler
static void _pxprpc__stepGetFunc1(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    r->temp1=-1;
    int lenOut=-1;
    for(int i=0;i<self->lengthOfNamedFuncs;i++){
        lenOut=-1;
        int cmp1=_pxprpc__strcmp2(self->namedfuncs[i].name,&lenOut,(const char *)r->parameter.base,&r->parameter.length);
        if(!cmp1){
            pxprpc_ref *ref2=pxprpc_alloc_ref(self,&r->temp1);
            ref2->object=self->namedfuncs[i].callable;
            break;
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
            if(req2!=NULL && (req2->session&0xffffff00)==(req->session&0xffffff00)){
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

#include <stdio.h>
static void _pxprpc__step2(struct _pxprpc__ServCo *self){
    self->exp.last_error=self->io1->get_error(self->io1,(void *)self->io1->receive);
    if(self->exp.last_error!=NULL){
        pxprpc_close(self);
        return;
    }
    pxprpc_request *req=_pxprpc__newRequest(self,self->hdr.session);
    req->callable_index=self->hdr.callableIndex;
    req->parameter=self->recvBuf[1].bytes;
    
    if(self->hdr.callableIndex>=0){
        req->next_step=_pxprpc__stepCall1;
    }else{
        switch(-self->hdr.callableIndex){
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
    self->recvBuf[0].bytes.length=8;
    self->recvBuf[0].bytes.base=self->hdrbuf;
    self->recvBuf[0].next_part=&self->recvBuf[1];
    self->recvBuf[1].bytes.base=NULL;
    self->recvBuf[1].bytes.length=0;
    self->io1->receive(self->io1,&self->recvBuf[0],(void *)_pxprpc__step2,self);
}



static int pxprpc_new_server_context(pxprpc_server_context *server_context,struct pxprpc_abstract_io *io1,struct pxprpc_namedfunc *namedfuncs,int len_namedfuncs){
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)pxprpc__malloc(sizeof(struct _pxprpc__ServCo));
    memset(ctx,0,sizeof(struct _pxprpc__ServCo));
    ctx->io1=io1;
    ctx->namedfuncs=namedfuncs;
    ctx->lengthOfNamedFuncs=len_namedfuncs;
    *server_context=ctx;
    return 0;
}

static int pxprpc_start_serve(pxprpc_server_context server_context){
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)server_context;
    _pxprpc__ServCoStart(ctx);
    return 0;
}

static int pxprpc_free_context(pxprpc_server_context *server_context){
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)*server_context;
    pxprpc_close(ctx);
    pxprpc__free(*server_context);
    server_context=NULL;
    return 0;
}

static struct pxprpc_server_context_exports *pxprpc_server_context_exports(pxprpc_server_context context){
    struct _pxprpc__ServCo *self=context;
    return &self->exp;
}

static pxprpc_server_api exports={
    &pxprpc_new_server_context,&pxprpc_start_serve,&pxprpc_closed,&pxprpc_close,&pxprpc_free_context,
    &pxprpc_alloc_ref,&pxprpc_free_ref,&pxprpc_get_ref,&pxprpc_server_context_exports
};

extern int pxprpc_server_query_interface(pxprpc_server_api **outapi){
    *outapi=&exports;
}


#endif