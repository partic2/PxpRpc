

#include "def.h"
#include "config.h"
#include <stdint.h>

#include "memory.h"

#include "server.h"



static int _pxprpc___ref_AddRef(struct pxprpc_object *ref){
    ref->count++;
    return ref->count;
}

static int _pxprpc__ref_Release(struct pxprpc_object *ref){
    ref->count--;
    if(ref->count==0){
        pxprpc__free(ref);
        return 0;
    }else{
        return ref->count;
    }
}

static int _pxprpc__ref_bytes_Release(struct pxprpc_object *ref){
    void *bytes=ref->object1;
    if(_pxprpc__ref_Release(ref)==0){
        pxprpc__free(bytes);
    }
}


extern struct pxprpc_object *pxprpc_new_object(void *obj){
    struct pxprpc_object *ref=pxprpc__malloc(sizeof(struct pxprpc_object));
    ref->addRef=&_pxprpc___ref_AddRef;
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

struct _pxprpc__ServCo{
    void (*next)(struct _pxprpc__ServCo *self);
    uint8_t *buf;
    uint32_t session;
    int readLength;
    struct pxprpc_abstract_io *io1;
    struct pxprpc_object **refSlots;
    int slotMaxHashOffset;
    int slotsSize;
    struct pxprpc_object *t1;
};

static void _pxprpc__ServCoStart(struct _pxprpc__ServCo *self){
    self->buf=pxprpc__malloc(256); 
    self->slotsSize=MAX_REFSLOTS_COUNT;
    self->refSlots=pxprpc__malloc(sizeof(struct pxprpc_object)*MAX_REFSLOTS_COUNT);
    memset(self->refSlots,0,sizeof(struct pxprpc_object)*MAX_REFSLOTS_COUNT);
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

static void _pxprpc__step1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,4,self->buf,&_pxprpc__step2,self);
}

static void _pxprpc__step2(struct _pxprpc__ServCo *self){
    self->session=*(uint32_t *)self->buf;
    int opcode=self->buf[0];
    switch(opcode){
        case 1:
        self->next=&_pxprpc__stepPush1;
        return;
        case 2:
    }
}

//Push handler
static void _pxprpc__stepPush1(struct _pxprpc__ServCo *self){
    self->io1->read(self->io1,8,self->buf+4,_pxprpc__stepPush2,self);
}
static void _pxprpc__stepPush2(struct _pxprpc__ServCo *self){
    //XXX: limit to Little-Endian
    int length=*((uint32_t *)self->buf+2);
    self->t1=pxprpc_new_bytes_object(length);

    self->io1->read(self->io1,4,&((struct pxprpc_bytes *)self->t1->object1)->data,
      _pxprpc__stepPush3,self);
}
static void _pxprpc__stepPush3(struct _pxprpc__ServCo *self){
    int destAddr=*((uint32_t *)self->buf+3);
    _pxprpc__RefSlotsPut(self,destAddr,self->t1);
    self->io1->write(self->io1,4,self->buf);
}
