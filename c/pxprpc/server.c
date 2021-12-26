

#include "def.h"
#include "config.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#include "server.h"

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
    if(_pxprpc__ref_Release(ref)==0){
        pxprpc__free(bytes);
    }
}



struct _pxprpc__ServCo{
    void (*next)(struct _pxprpc__ServCo *self);
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
    struct pxprpc_abstract_io *io1;
    struct pxprpc_object **refSlots;
    int slotMaxHashOffset;
    int slotsSize;
    struct pxprpc_object *t1;
    pthread_mutex_t writeMutex;
    int lengthOfNamedFuncs;
    struct pxprpc_namedfunc *namedfuncs;
    uint32_t status;
};

#define _pxprpc__ServCoIsClosed(self) self->status&0x1

static void _pxprpc__step1(struct _pxprpc__ServCo *self);

static void _pxprpc__ServCoStart(struct _pxprpc__ServCo *self){
    self->slotsSize=MAX_REFSLOTS_COUNT;
    self->refSlots=pxprpc__malloc(sizeof(struct pxprpc_object)*MAX_REFSLOTS_COUNT);
    memset(self->refSlots,0,sizeof(struct pxprpc_object)*MAX_REFSLOTS_COUNT);
    pthread_mutex_init(&self->writeMutex,NULL);
    self->status=0;
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


//Push handler
static void _pxprpc__stepPush3(struct _pxprpc__ServCo *self){
    _pxprpc__RefSlotsPut(self,self->hdr.addr1,self->t1);
    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->hdr.session);
    pthread_mutex_unlock(&self->writeMutex);

    _pxprpc__step1(self);
}
static void _pxprpc__stepPush2(struct _pxprpc__ServCo *self){
    self->t1=pxprpc_new_bytes_object(self->hdr.length1);

    self->io1->read(self->io1,self->hdr.length1,((struct pxprpc_bytes *)self->t1->object1)->data,
      (void (*)(void *))&_pxprpc__stepPush3,self);
}
static void _pxprpc__stepPush1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepPush2,self);
}

//Pull handler
static void _pxprpc__stepPull2(struct _pxprpc__ServCo *self){
    int destAddr=self->hdr.addr1;
    
    struct pxprpc_bytes *bytes=self->refSlots[destAddr]->object1;

    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->hdr.session);
    self->io1->write(self->io1,4,(uint8_t *)&bytes->length);
    self->io1->write(self->io1,bytes->length,bytes->data);
    pthread_mutex_unlock(&self->writeMutex);

    _pxprpc__step1(self);
}
static void _pxprpc__stepPull1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepPull2,self);
}

//Assign handler
static void _pxprpc__stepAssign2(struct _pxprpc__ServCo *self){
    _pxprpc__RefSlotsPut(self,self->hdr.addr1,self->refSlots[self->hdr.addr2]);
    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->hdr.session);
    pthread_mutex_unlock(&self->writeMutex);

    _pxprpc__step1(self);
}
static void _pxprpc__stepAssign1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepAssign2,self);
}


//Unlink handler
static void _pxprpc__stepUnlink2(struct _pxprpc__ServCo *self){
    _pxprpc__RefSlotsPut(self,self->hdr.addr1,NULL);
    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->hdr.session);
    pthread_mutex_unlock(&self->writeMutex);

    _pxprpc__step1(self);
}
static void _pxprpc__stepUnlink1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepUnlink2,self);
}


//Call handler
static void _pxprpc__stepCall4(struct pxprpc_request *r, struct pxprpc_object *result){
    _pxprpc__RefSlotsPut(r->server_context_data,r->dest_addr,result);
    r->result=result;
    struct _pxprpc__ServCo *self=r->server_context_data;
    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->hdr.session);
    r->callable->writeResult(r->callable,r);
    pthread_mutex_unlock(&self->writeMutex);
    pxprpc__free(r);
}

static void _pxprpc__stepCall3(struct pxprpc_request *r){
    r->callable->call(r->callable,r,&_pxprpc__stepCall4);
    struct _pxprpc__ServCo *self=r->server_context_data;
    _pxprpc__step1(self);
}
static void _pxprpc__stepCall2(struct _pxprpc__ServCo *self){
    struct pxprpc_request *req=pxprpc__malloc(sizeof(struct pxprpc_request));
    req->session=self->hdr.session;
    req->io1=self->io1;
    req->ref_slots=self->refSlots;
    req->dest_addr=self->hdr.addr1;
    req->server_context_data=self;
    struct pxprpc_callable *func=(struct pxprpc_callable *)((struct pxprpc_object *)self->refSlots[self->hdr.addr2])->object1;
    func->readParameter(func,req,&_pxprpc__stepCall3);
}
static void _pxprpc__stepCall1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void (*)(void *))&_pxprpc__stepCall2,self);
}


//GetFunc handler
static void _pxprpc__stepGetFunc2(struct _pxprpc__ServCo *self){
    struct pxprpc_bytes *bs=self->refSlots[self->hdr.addr2]->object1;
    //Avoid buffer overflow
    if(bs->data[bs->length-1]!=0){
        bs->data[bs->length-1]=0;
    }
    int returnAddr=0;
    for(int i=0;i<self->lengthOfNamedFuncs;i++){
        if(strcmp(self->namedfuncs[i].name,bs->data)){
            _pxprpc__RefSlotsPut(self,self->hdr.addr1,
              pxprpc_new_object(self->namedfuncs->callable));
            returnAddr=self->hdr.addr1;
        }
        break;
    }
    pthread_mutex_lock(&self->writeMutex);
    self->io1->write(self->io1,4,(uint8_t *)&self->hdr.session);
    self->io1->write(self->io1,4,(uint8_t *)&returnAddr);
    pthread_mutex_unlock(&self->writeMutex);

    _pxprpc__step1(self);
}
static void _pxprpc__stepGetFunc1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,(uint8_t*)&self->hdr.addr1,(void(*)(void *))&_pxprpc__stepGetFunc2,self);
}


//Close handler
static void _pxprpc__stepClose1(struct _pxprpc__ServCo *self){
    pxprpc_close(self);
}



//GetInfo handler
static void _pxprpc__stepGetInfo1(struct _pxprpc__ServCo *self){
    pthread_mutex_lock(&self->writeMutex);
    uint32_t length=strlen(ServerInfo);
    self->io1->write(self->io1,4,(uint8_t *)&length);
    self->io1->write(self->io1,length,ServerInfo);
    pthread_mutex_unlock(&self->writeMutex);
}



static void _pxprpc__step1(struct _pxprpc__ServCo *self){
    if(_pxprpc__ServCoIsClosed(self)){
        return;
    }
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


extern void pxprpc_close(void *server_context){
    struct _pxprpc__ServCo *self=server_context;
    for(int i=0;i<MAX_REFSLOTS_COUNT;i++){
        if(self->refSlots[i]!=NULL){
            self->refSlots[i]->release(self->refSlots[i]);
        }
    }
    self->status|=0x1;
}


extern struct pxprpc_object *pxprpc_new_object(void *obj){
    struct pxprpc_object *ref=pxprpc__malloc(sizeof(struct pxprpc_object));
    ref->addRef=&_pxprpc__ref_AddRef;
    ref->release=&_pxprpc__ref_Release;
    ref->count=0;
    ref->object1=obj;
    ref->size_of_struct=sizeof(struct pxprpc_object);
    return ref;
}

extern struct pxprpc_object *pxprpc_new_bytes_object(uint32_t size){
    struct pxprpc_object *ref=pxprpc_new_object(pxprpc__malloc(size+4));
    *((int *)(ref->object1))=size;
    ref->release=_pxprpc__ref_bytes_Release;
    return ref;
}

extern int pxprpc_new_server_context(pxprpc_server_context *server_context,struct pxprpc_abstract_io *io1,struct pxprpc_namedfunc *namedfuncs,int len_namedfuncs){
    struct _pxprpc__ServCo *ctx=pxprpc__malloc(sizeof(struct _pxprpc__ServCo));
    ctx->io1=io1;
    ctx->namedfuncs=namedfuncs;
    ctx->lengthOfNamedFuncs=len_namedfuncs;
}

extern int pxprpc_start_serve(pxprpc_server_context server_context){
    struct _pxprpc__ServCo *ctx=server_context;
    _pxprpc__ServCoStart(ctx);
}

extern int pxprpc_free_context(pxprpc_server_context *server_context){
    struct _pxprpc__ServCo *ctx=(struct _pxprpc__ServCo *)*server_context;
    if(!_pxprpc__ServCoIsClosed(ctx)){
        pxprpc_close(ctx);
    }
    pxprpc__free(server_context);
}
