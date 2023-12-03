#ifndef _PXPRPC_SERVER_C
#define _PXPRPC_SERVER_C

#include "utils.c"

#include <pxprpc.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>


#ifdef USE_PTHREAD_HEADER
#include <pthread.h>
#endif

#define MACRO_TO_STR(x) #x
#define MACRO_TO_STR2(s) MACRO_TO_STR(s)


static const char *ServerInfo="server name:pxprpc for c\n"
"version:1.0\n"
"reference slots size:" MACRO_TO_STR2(MAX_REFSLOTS_COUNT)"\n";



static uint32_t _pxprpc__ref_AddRef(struct pxprpc_object *ref){
    ref->count++;
    return ref->count;
}

static uint32_t _pxprpc__ref_Release(struct pxprpc_object *ref){
    ref->count--;
    if(ref->count==0){
        pxprpc__free(ref);
        return 0;
    }else{
        return ref->count;
    }
}

static uint32_t _pxprpc__ref_bytes_Release(struct pxprpc_object *ref){
    void *bytes=ref->object1;
    if(ref->count==1){
        pxprpc__free(bytes);
    }
    return _pxprpc__ref_Release(ref);
}

static struct pxprpc_object *pxprpc_new_object(void *obj){
    struct pxprpc_object *ref=pxprpc__malloc(sizeof(struct pxprpc_object));
    ref->addRef=&_pxprpc__ref_AddRef;
    ref->release=&_pxprpc__ref_Release;
    ref->count=0;
    ref->object1=obj;
    return ref;
}

static struct pxprpc_object *pxprpc_new_bytes_object(uint32_t size){
    struct pxprpc_object *ref=pxprpc_new_object(pxprpc__malloc(size+4));
    ref->type=1;
    *((int *)(ref->object1))=size;
    ref->release=_pxprpc__ref_bytes_Release;
    return ref;
}

static void pxprpc_fill_bytes_object(struct pxprpc_object *obj,uint8_t *data,int size){
    struct pxprpc_bytes * bytes=(struct pxprpc_bytes *)obj->object1;
    if(size>bytes->length){
        size=bytes->length;
    }
    memcpy(&bytes->data,data,size);
}

struct _pxprpc__ServCo{
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
    struct pxprpc_abstract_io *io1;
    struct pxprpc_object **refSlots;
    int slotMaxHashOffset;
    int slotsSize;
    struct pxprpc_object *t1;
    pthread_mutex_t writeMutex;
    int lengthOfNamedFuncs;
    struct pxprpc_namedfunc *namedfuncs;
    uint32_t status;
    uint8_t writeBuf[4];
    uint32_t responseSession;
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
        if(self->refSlots[i]!=NULL){
            self->refSlots[i]->release(self->refSlots[i]);
        }
    }
    self->status|=0x1;
    pxprpc__free(self->refSlots);
    return 0;
}


static void _pxprpc__step1(struct _pxprpc__ServCo *self);

static void _pxprpc__ServCoStart(struct _pxprpc__ServCo *self){
    self->slotsSize=MAX_REFSLOTS_COUNT;
    self->refSlots=pxprpc__malloc(sizeof(struct pxprpc_object)*MAX_REFSLOTS_COUNT);
    memset(self->refSlots,0,sizeof(struct pxprpc_object)*MAX_REFSLOTS_COUNT);
    pthread_mutex_init(&self->writeMutex,NULL);
    self->status=0;
    _pxprpc__step1(self);
}

static void _pxprpc__RefSlotsPut(struct _pxprpc__ServCo *self,uint32_t addr, struct pxprpc_object *ref2){
    struct pxprpc_object *ref=self->refSlots[addr];
    if(ref!=NULL){
        ref->release(ref);
    }
    if(ref2!=NULL){
        ref2->addRef(ref2);
    }
    self->refSlots[addr]=ref2; 
}

static void _pxprpc__nopcallback(void *p){}

static void _pxprpc__responseStart(struct _pxprpc__ServCo *self,uint32_t session){
    self->responseSession=session;
    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->responseSession,_pxprpc__nopcallback,self);
}

static void _pxprpc__responseStop(struct _pxprpc__ServCo *self){
    pthread_mutex_unlock(&self->writeMutex);
}

//Push handler
static void _pxprpc__stepPush3(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    _pxprpc__RefSlotsPut(self,self->hdr.addr1,self->t1);
    _pxprpc__responseStart(self,self->hdr.session);
    _pxprpc__responseStop(self);
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
static void _pxprpc__stepPull2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    int destAddr=self->hdr.addr1;
    
    struct pxprpc_bytes *bytes=self->refSlots[destAddr]->object1;

    _pxprpc__responseStart(self,self->hdr.session);
    if(bytes!=NULL){
        self->io1->write(self->io1,4,(uint8_t *)&bytes->length,_pxprpc__nopcallback,NULL);
        self->io1->write(self->io1,bytes->length,bytes->data,(void (*)(void *))_pxprpc__step1,self);
    }else{
        *(uint32_t *)(&self->writeBuf)=0xffffffff;
        self->io1->write(self->io1,4,self->writeBuf,(void (*)(void *))_pxprpc__step1,self);
    }
    _pxprpc__responseStop(self);
}
static void _pxprpc__stepPull1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepPull2,self);
}

//Assign handler
static void _pxprpc__stepAssign2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    _pxprpc__RefSlotsPut(self,self->hdr.addr1,self->refSlots[self->hdr.addr2]);
    _pxprpc__responseStart(self,self->hdr.session);
    _pxprpc__responseStop(self);
    _pxprpc__step1(self);
}
static void _pxprpc__stepAssign1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepAssign2,self);
}


//Unlink handler
static void _pxprpc__stepUnlink2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    _pxprpc__RefSlotsPut(self,self->hdr.addr1,NULL);
    _pxprpc__responseStart(self,self->hdr.session);
    _pxprpc__responseStop(self);
    _pxprpc__step1(self);
}
static void _pxprpc__stepUnlink1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepUnlink2,self);
}

//Call Handler
static void _pxprpc__stepCall4(struct pxprpc_request *r, struct pxprpc_object *result){
    struct _pxprpc__ServCo *self=r->server_context;
    _pxprpc__RefSlotsPut(r->server_context,r->dest_addr,result);
    r->result=result;
    _pxprpc__responseStart(self,r->session);
    r->callable->writeResult(r->callable,r);
    _pxprpc__responseStop(self);
    pxprpc__free(r);
}

static void _pxprpc__stepCall3(struct pxprpc_request *r){
    struct _pxprpc__ServCo *self=(struct _pxprpc__ServCo *)r->server_context;
    r->callable->call(r->callable,r,&_pxprpc__stepCall4);
    _pxprpc__step1(self);
}
static void _pxprpc__stepCall2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    struct pxprpc_request *req=(struct pxprpc_request *)pxprpc__malloc(sizeof(struct pxprpc_request));
    req->session=self->hdr.session;
    req->io1=self->io1;
    req->ref_slots=self->refSlots;
    req->dest_addr=self->hdr.addr1;
    req->server_context=self;
    struct pxprpc_callable *func=(struct pxprpc_callable *)((struct pxprpc_object *)self->refSlots[self->hdr.addr2])->object1;
    req->callable=func;
    func->readParameter(func,req,&_pxprpc__stepCall3);
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
static void _pxprpc__stepGetFunc2(struct _pxprpc__ServCo *self){
    if(self->io1->get_error(self->io1,self->io1->read)!=NULL)return;
    struct pxprpc_bytes *bs=(struct pxprpc_bytes *)self->refSlots[self->hdr.addr2]->object1;
    int returnAddr=0;
    int lenOut=-1;
    for(int i=0;i<self->lengthOfNamedFuncs;i++){
        lenOut=-1;
        int cmp1=_pxprpc__strcmp2(self->namedfuncs[i].name,&lenOut,(const char *)bs->data,&bs->length);
        if(!cmp1){
            _pxprpc__RefSlotsPut(self,self->hdr.addr1,pxprpc_new_object(self->namedfuncs[i].callable));
            returnAddr=self->hdr.addr1;
            break;
        }
    }
    _pxprpc__responseStart(self,self->hdr.session);
    *(uint32_t *)(&self->writeBuf)=returnAddr;
    self->io1->write(self->io1,4,self->writeBuf,(void (*)(void *))_pxprpc__step1,self);
    _pxprpc__responseStop(self);
}
static void _pxprpc__stepGetFunc1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void(*)(void *))&_pxprpc__stepGetFunc2,self);
}


//Close handler
static int _pxprpc__stepClose1(struct _pxprpc__ServCo *self){
    pxprpc_close(self);
}



//GetInfo handler
static void _pxprpc__stepGetInfo1(struct _pxprpc__ServCo *self){
    _pxprpc__responseStart(self,self->hdr.session);
    uint32_t length=strlen(ServerInfo);
    *(uint32_t *)(&self->writeBuf)=length;
    self->io1->write(self->io1,4,(uint8_t *)&length,_pxprpc__nopcallback,NULL);
    self->io1->write(self->io1,length,ServerInfo,(void (*)(void *))_pxprpc__step1,self);
    _pxprpc__responseStop(self);
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
    return 0;
}


static pxprpc_server_api exports={
    &pxprpc_new_server_context,&pxprpc_start_serve,&pxprpc_closed,&pxprpc_close,&pxprpc_free_context,
    &pxprpc_new_object,&pxprpc_new_bytes_object,&pxprpc_fill_bytes_object
};

extern int pxprpc_server_query_interface(pxprpc_server_api **outapi){
    *outapi=&exports;
}


#endif