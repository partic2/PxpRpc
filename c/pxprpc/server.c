

#include "def.h"
#include "config.h"

#include "memory.h"

struct _pxprpc__RefSlot{
    int addr;
    void *value;
};

struct _pxprpc__ServCo{
    void (*next)(struct _pxprpc__ServCo *self);
    uint8_t *buf;
    uint8_t *largeBuf;
    uint32_t session;
    int readLength;
    struct pxprpc_abstract_io *io1;
    struct _pxprpc__RefSlot *refSlots;
};

static void _pxprpc__ServCoStart(struct _pxprpc__ServCo *self){
    self->buf=pxprpc__malloc(512); 
    self->refSlots=pxprpc__malloc(sizeof(struct _pxprpc__RefSlot)*256);
    memset(self->refSlots,0,sizeof(struct _pxprpc__RefSlot)*256);
}

static void _pxprpc__RefSlotsPut(struct _pxprpc__ServCo *self,uint32_t addr,void *value){
    int putAddr=addr&0xff;
    if(self->refSlots[putAddr].addr==NULL||self->refSlots[putAddr].addr==addr){
        self->refSlots[putAddr].addr=addr;
        self->refSlots[putAddr].value=value;
    }else{
        
    }
}
static void _pxprpc__RefSlotsRemove(int addr,void *value){
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

static void _pxprpc__stepPush1(struct _pxprpc__ServCo *self){
    self->session=*(uint32_t *)self->buf;
    int opcode=self->buf[0];
    switch(opcode){
        case 1:
    }
}