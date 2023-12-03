#ifndef _PXPRPC_UTILS_C
#define _PXPRPC_UTILS_C

/* config */

/* use include pthread to create mutex */
#define USE_PTHREAD_HEADER

#define MAX_REFSLOTS_COUNT 256

#include <pxprpc.h>

#define BUFFER_CAPACITY 1024

struct _pxprpc__bufferedIo{
    struct pxprpc_abstract_io io1;
    struct pxprpc_abstract_io *wrapped;
    void *buffer;
    uint32_t pos;
    const uint8_t *writingBuf;
    uint32_t writingBufLen;
    void (*onCompleted)(void *args);
    void *p;
    char flush;
};

static void _pxprpc__bufferedIoRead(struct pxprpc_abstract_io *self,uint32_t length,uint8_t *buf,void (*onCompleted)(void *args),void *p){
    struct _pxprpc__bufferedIo *self2=self->userData;
    self2->wrapped->read(self2->wrapped,length,buf,onCompleted,p);
}
static void _pxprpc__bufferedIoWrite(struct pxprpc_abstract_io *self,uint32_t length,const uint8_t *buf,void (*onCompleted)(void *args),void *p){
    struct _pxprpc__bufferedIo *self2=self->userData;
    self2->wrapped->write(self2->wrapped,length,buf,onCompleted,p);
}

static void _pxprpc__bufferedIoBufferedWriteStep2(struct _pxprpc__bufferedIo *self){
    if(self->flush){
        self->pos=0;
        if(self->writingBufLen>BUFFER_CAPACITY){
            self->wrapped->write(self->wrapped,self->writingBufLen,self->writingBuf,self->onCompleted,self->p);
            return;
        }
    }
    memmove(self->buffer+self->pos,self->writingBuf,self->writingBufLen);
    self->pos+=self->writingBufLen;
    self->onCompleted(self->p);
}

static void _pxprpc__bufferedIoBufferedWriteStep1(struct pxprpc_abstract_io *self2,uint32_t length,const uint8_t *buf,void (*onCompleted)(void *args),void *p){
    struct _pxprpc__bufferedIo *self=self2->userData;
    self->writingBuf=buf;
    self->writingBufLen=length;
    self->onCompleted=onCompleted;
    self->p=p;
    if(length>BUFFER_CAPACITY-self->pos){
        self->flush=1;
        self->wrapped->write(self->wrapped,self->pos,self->buffer,(void *)_pxprpc__bufferedIoBufferedWriteStep2,self);
    }else{
        self->flush=0;
        _pxprpc__bufferedIoBufferedWriteStep2(self);
    }
}

static void _pxprpc__bufferedIoInit(struct _pxprpc__bufferedIo *self,struct pxprpc_abstract_io *wrapped){
    memset(self,0,sizeof(struct _pxprpc__bufferedIo));
    self->buffer=NULL;
    self->pos=0;
    self->wrapped=wrapped;
    self->io1.read=_pxprpc__bufferedIoRead;
    self->io1.write=_pxprpc__bufferedIoWrite;
    self->io1.userData=self;
    self->io1.get_error=wrapped->get_error;
}

static void _pxprpc__bufferedIoSetBufStep2(struct _pxprpc__bufferedIo *self){
    pxprpc__free(self->buffer);
    self->buffer=NULL;
    self->onCompleted(self->p);
}

static void _pxprpc__bufferedIoSetBuf(struct _pxprpc__bufferedIo *self,int enabled,void (*onCompleted)(void *args),void *p){
    self->onCompleted=onCompleted;
    self->p;
    if(self->buffer==NULL && enabled){
        self->buffer=pxprpc__malloc(BUFFER_CAPACITY);
        self->io1.write=_pxprpc__bufferedIoBufferedWriteStep1;
        self->onCompleted(self->p);
    }else if(self->buffer!=NULL && !enabled){
        self->io1.write=_pxprpc__bufferedIoWrite;
        self->flush=1;
        self->wrapped->write(self->wrapped,self->pos,self->buffer,(void *)_pxprpc__bufferedIoSetBufStep2,self);
    }
}


#endif
