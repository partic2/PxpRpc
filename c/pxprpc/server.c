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
"version:1.1\n"
"reference slots size:" MACRO_TO_STR2(MAX_REFSLOTS_COUNT)"\n";

static struct pxprpc_server_context_exports *pxprpc_server_context_exports(pxprpc_server_context context);

static uint32_t _pxprpc__ref_AddRef(pxprpc_object *ref){
    ref->count++;
    return ref->count;
}

static uint32_t _pxprpc__ref_Release(pxprpc_object *ref){
    ref->count--;
    if(ref->count==0){
        pxprpc__free(ref);
        return 0;
    }else{
        return ref->count;
    }
}

static uint32_t _pxprpc__ref_bytes_Release(pxprpc_object *ref){
    void *bytes=ref->object1;
    if(ref->count==1){
        pxprpc__free(bytes);
    }
    return _pxprpc__ref_Release(ref);
}

static pxprpc_object *pxprpc_new_object(void *obj){
    pxprpc_object *ref=pxprpc__malloc(sizeof(pxprpc_object));
    ref->addRef=&_pxprpc__ref_AddRef;
    ref->release=&_pxprpc__ref_Release;
    ref->count=0;
    ref->object1=obj;
    return ref;
}

static pxprpc_object *pxprpc_new_bytes_object(uint32_t size){
    pxprpc_object *ref=pxprpc_new_object(pxprpc__malloc(size+4));
    ref->type=1;
    *((int *)(ref->object1))=size;
    ref->release=_pxprpc__ref_bytes_Release;
    return ref;
}

static void pxprpc_fill_bytes_object(pxprpc_object *obj,uint8_t *data,int size){
    struct pxprpc_bytes * bytes=(struct pxprpc_bytes *)obj->object1;
    if(size>bytes->length){
        size=bytes->length;
    }
    memcpy(&bytes->data,data,size);
}

struct _pxprpc__ServCo{
    struct pxprpc_abstract_io *io1;
    struct pxprpc_server_context_exports exp;
    int lengthOfNamedFuncs;
    struct pxprpc_namedfunc *namedfuncs;
    uint32_t status;
    uint8_t writeBuf[4];
    uint32_t sequenceSessionMask;
    char sequenceMaskBitsCnt;
    pxprpc_object *t1;
    #pragma pack(1)
    union{
        struct{
            uint32_t session;
            uint32_t addr1;
            union{
                uint32_t addr2;
                uint32_t length1;
            };
        } hdr;
        uint8_t hdrbuf[16];
    };
    #pragma pack()
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
    for(int i=0;i<MAX_REFSLOTS_COUNT;i++){
        if(self->exp.ref_slots[i]!=NULL){
            self->exp.ref_slots[i]->release(self->exp.ref_slots[i]);
        }
    }
    self->status|=0x1;
    return 0;
}


static void _pxprpc__step1(struct _pxprpc__ServCo *self);

static void _pxprpc__ServCoStart(struct _pxprpc__ServCo *self){
    self->exp.slots_size=MAX_REFSLOTS_COUNT;
    self->exp.ref_slots=pxprpc__malloc(sizeof(pxprpc_object *)*MAX_REFSLOTS_COUNT);
    self->pendingReqTail=NULL;
    self->sequenceSessionMask=0xffffffff;
    memset(self->exp.ref_slots,0,sizeof(pxprpc_object *)*MAX_REFSLOTS_COUNT);
    self->status=0;
    _pxprpc__step1(self);
}

static void _pxprpc__RefSlotsPut(struct _pxprpc__ServCo *self,uint32_t addr, pxprpc_object *ref2){
    pxprpc_object *ref=self->exp.ref_slots[addr];
    if(ref!=NULL){
        ref->release(ref);
    }
    if(ref2!=NULL){
        ref2->addRef(ref2);
    }
    self->exp.ref_slots[addr]=ref2; 
}

static void _pxprpc__nopcallback(void *p){}

static void _pxprpc__responseStart(pxprpc_request *r,void (*onCompleted)(void *),void *p){
    struct _pxprpc__ServCo *self=r->server_context;
    self->io1->write(self->io1,4,(const uint8_t *)&r->session,onCompleted,p);
}


static void _pxprpc__queueRequest(pxprpc_request *r){
    struct _pxprpc__ServCo *self=r->server_context;
    if(self->sequenceSessionMask==0xffffffff || self->sequenceSessionMask!=(r->session>>(32-self->sequenceMaskBitsCnt)) ){
        r->inSequence=0;
        r->nextStep(r);
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
            r->nextStep(r);
        }
    }
}

static void _pxprpc__finishRequest(pxprpc_request *r){
    r->finishCount--;
    if(r->finishCount)return;
    if(r->inSequence){
        if(r->lastReq!=NULL){
            r->lastReq->nextReq=r->nextReq;
        }
        if(r->nextReq!=NULL){
            r->nextReq->lastReq=r->lastReq;
            if((r->nextReq->session&0xffffff00)==(r->session&0xffffff00)){
                r->nextReq->nextStep(r->nextReq);
            }
        }
    }
    pxprpc__free(r);
}

static pxprpc_request *_pxprpc__newRequest(pxprpc_server_context context,uint32_t session){
    pxprpc_request *r=pxprpc__malloc(sizeof(pxprpc_request));
    r->session=session;
    r->server_context=context;
    r->finishCount=1;
    r->nextReq=NULL;
    r->lastReq=NULL;
    return r;
}

//Push handler
static void _pxprpc__stepPush4(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    _pxprpc__RefSlotsPut(self,r->dest_addr,(pxprpc_object *)r->callable_data);
    _pxprpc__responseStart(r,(void *)&_pxprpc__finishRequest,r);
}
static void _pxprpc__stepPush3(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->callable_data=self->t1;
    r->dest_addr=self->hdr.addr1;
    r->nextStep=_pxprpc__stepPush4;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepPush2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    self->t1=pxprpc_new_bytes_object(self->hdr.length1);
    self->io1->read(self->io1,self->hdr.length1,((struct pxprpc_bytes *)self->t1->object1)->data,
      (void (*)(void *))&_pxprpc__stepPush3,self);
}
static void _pxprpc__stepPush1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepPush2,self);
}

//Pull handler
static void _pxprpc__stepPull3(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    struct pxprpc_bytes *bytes=pxprpc_server_context_exports(r->server_context)->ref_slots[r->dest_addr]->object1;

    _pxprpc__responseStart(r,_pxprpc__nopcallback,NULL);
    if(bytes!=NULL){
        self->io1->write(self->io1,4,(uint8_t *)&bytes->length,_pxprpc__nopcallback,NULL);
        self->io1->write(self->io1,bytes->length,bytes->data,(void *)_pxprpc__finishRequest,r);
    }else{
        *(uint32_t *)(&self->writeBuf)=0xffffffff;
        self->io1->write(self->io1,4,self->writeBuf,(void *)_pxprpc__finishRequest,r);
    }
}
static void _pxprpc__stepPull2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->dest_addr=self->hdr.addr1;
    r->nextStep=_pxprpc__stepPull3;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepPull1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepPull2,self);
}

//Assign handler
static void _pxprpc__stepAssign3(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    _pxprpc__RefSlotsPut(self,r->dest_addr,*(pxprpc_object **)r->callable_data);
    _pxprpc__responseStart(r,(void *)&_pxprpc__finishRequest,r);
}
static void _pxprpc__stepAssign2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->dest_addr=self->hdr.addr1;
    r->callable_data=self->exp.ref_slots+self->hdr.addr2;
    r->nextStep=_pxprpc__stepAssign3;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepAssign1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepAssign2,self);
}


//Unlink handler
static void _pxprpc__stepUnlink3(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    _pxprpc__RefSlotsPut(self,r->dest_addr,NULL);
    _pxprpc__responseStart(r,(void *)&_pxprpc__finishRequest,r);
}
static void _pxprpc__stepUnlink2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->dest_addr=self->hdr.addr1;
    r->callable_data=self->exp.ref_slots+self->hdr.addr2;
    r->nextStep=_pxprpc__stepUnlink3;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepUnlink1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepUnlink2,self);
}

//Call Handler
static void _pxprpc__stepCall5(pxprpc_request *r, pxprpc_object *result){
    struct _pxprpc__ServCo *self=r->server_context;
    _pxprpc__RefSlotsPut(r->server_context,r->dest_addr,result);
    r->result=result;
    r->finishCount=2;
    _pxprpc__responseStart(r,(void *)&_pxprpc__finishRequest,r);
    r->callable->writeResult(r->callable,r,_pxprpc__finishRequest);
}

static void _pxprpc__stepCall4(pxprpc_request *r){
    r->callable->call(r->callable,r,&_pxprpc__stepCall5);
}

static void _pxprpc__stepCall3(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    r->nextStep=_pxprpc__stepCall4;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepCall2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->io1=self->io1;
    r->dest_addr=self->hdr.addr1;
    pxprpc_callable *func=(pxprpc_callable *)((pxprpc_object *)self->exp.ref_slots[self->hdr.addr2])->object1;
    r->callable=func;
    func->readParameter(func,r,&_pxprpc__stepCall3);
}
static void _pxprpc__stepCall1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepCall2,self);
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
static void _pxprpc__stepGetFunc3(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    struct pxprpc_bytes *bs=(struct pxprpc_bytes *)(*(pxprpc_object **)r->callable_data)->object1;
    int returnAddr=0;
    int lenOut=-1;
    for(int i=0;i<self->lengthOfNamedFuncs;i++){
        lenOut=-1;
        int cmp1=_pxprpc__strcmp2(self->namedfuncs[i].name,&lenOut,(const char *)bs->data,&bs->length);
        if(!cmp1){
            _pxprpc__RefSlotsPut(self,r->dest_addr,pxprpc_new_object(self->namedfuncs[i].callable));
            returnAddr=r->dest_addr;
            break;
        }
    }
    _pxprpc__responseStart(r,_pxprpc__nopcallback,NULL);
    *(uint32_t *)(&self->writeBuf)=returnAddr;
    self->io1->write(self->io1,4,self->writeBuf,(void *)_pxprpc__finishRequest,r);
}
static void _pxprpc__stepGetFunc2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->dest_addr=self->hdr.addr1;
    r->callable_data=self->exp.ref_slots+self->hdr.addr2;
    r->nextStep=_pxprpc__stepGetFunc3;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepGetFunc1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void(*)(void *))&_pxprpc__stepGetFunc2,self);
}


static void _pxprpc__stepClose2(pxprpc_request *r){
    pxprpc_close(r->server_context);
}
static void _pxprpc__stepClose1(struct _pxprpc__ServCo *self){
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->nextStep=_pxprpc__stepClose2;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}


//GetInfo handler
static void _pxprpc__stepGetInfo2(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    _pxprpc__responseStart(r,_pxprpc__nopcallback,NULL);
    uint32_t length=strlen(ServerInfo);
    *(uint32_t *)(&self->writeBuf)=length;
    self->io1->write(self->io1,4,(uint8_t *)&length,_pxprpc__nopcallback,NULL);
    self->io1->write(self->io1,length,ServerInfo,(void *)&_pxprpc__finishRequest,r);
}
static void _pxprpc__stepGetInfo1(struct _pxprpc__ServCo *self){
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->nextStep=_pxprpc__stepGetInfo2;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}


static void _pxprpc__stepBuffer1(struct _pxprpc__ServCo *self){
    /* buffer command just ignored now. */
}

static void _pxprpc__stepSequence3(pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    uint32_t sessionMask=r->dest_addr;
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
    _pxprpc__responseStart(r,(void *)_pxprpc__finishRequest,r);
}

static void _pxprpc__stepSequence2(struct _pxprpc__ServCo *self){
    pxprpc_request *r=_pxprpc__newRequest(self,self->hdr.session);
    r->dest_addr=self->hdr.addr1;
    r->nextStep=_pxprpc__stepSequence3;
    _pxprpc__queueRequest(r);
    _pxprpc__step1(self);
}
static void _pxprpc__stepSequence1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void *)&_pxprpc__stepSequence2,self);
}


#include <stdio.h>
static void _pxprpc__step2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    int opcode=self->hdrbuf[0];
    switch(opcode){
        case 1:
        _pxprpc__stepPush1(self);
        break;
        case 2:
        _pxprpc__stepPull1(self);
        break;
        case 3:
        _pxprpc__stepAssign1(self);
        break;
        case 4:
        _pxprpc__stepUnlink1(self);
        break;
        case 5:
        _pxprpc__stepCall1(self);
        break;
        case 6:
        _pxprpc__stepGetFunc1(self);
        break;
        case 7:
        pxprpc_close(self);
        break;
        case 8:
        _pxprpc__stepGetInfo1(self);
        break;
        case 9:
        _pxprpc__stepSequence1(self);
        break;
        case 10:
        _pxprpc__stepBuffer1(self);
        break;
    }
}

static void _pxprpc__step1(struct _pxprpc__ServCo *self){
    if(_pxprpc__ServCoIsClosed(self)){
        return;
    }
    self->io1->read(self->io1,4,self->hdrbuf,(void(*)(void *))_pxprpc__step2,self);
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
    &pxprpc_new_object,&pxprpc_new_bytes_object,&pxprpc_fill_bytes_object,&pxprpc_server_context_exports
};

extern int pxprpc_server_query_interface(pxprpc_server_api **outapi){
    *outapi=&exports;
}


#endif