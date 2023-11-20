
#include <pxprpc.h>
#include <stdlib.h>

static void _pxprpc_ensure_buf(struct pxprpc_serializer *ser,uint32_t remain){
    if(ser->cap-ser->pos<remain){
        int newsize=ser->pos+remain;
        newsize+=newsize>>1;
        ser->buf=pxprpc__realloc(ser->buf,newsize);
    }
}

extern void pxprpc_ser_prepare_ser(struct pxprpc_serializer *ser,int initBufSize){
    ser->buf=pxprpc__malloc(initBufSize);
    ser->cap=initBufSize;
    ser->pos=0;
}
extern void pxprpc_ser_prepare_unser(struct pxprpc_serializer *ser,void *buf,uint32_t size){
    ser->cap=size;
    ser->pos=0;
    ser->buf=buf;
}
extern uint8_t *pxprpc_ser_build(struct pxprpc_serializer *ser,uint32_t *size){
    *size=ser->pos;
    return ser->buf;
}
extern void pxprpc_ser_put_int(struct pxprpc_serializer *ser,int32_t *val,int count){
    _pxprpc_ensure_buf(ser,4*count);
    memmove(ser->buf+ser->pos,val,4*count);
    ser->pos+=4*count;
}
extern void pxprpc_ser_put_long(struct pxprpc_serializer *ser,int64_t *val,int count){
    _pxprpc_ensure_buf(ser,8*count);
    memmove(ser->buf+ser->pos,val,8*count);
    ser->pos+=8*count;
}
extern void pxprpc_ser_put_float(struct pxprpc_serializer *ser,float *val,int count){
    _pxprpc_ensure_buf(ser,4*count);
    memmove(ser->buf+ser->pos,val,4*count);
    ser->pos+=4*count;
}
extern void pxprpc_ser_put_double(struct pxprpc_serializer *ser,double *val,int count){
    _pxprpc_ensure_buf(ser,8*count);
    memmove(ser->buf+ser->pos,val,8*count);
    ser->pos+=8*count;
}
extern void pxprpc_ser_put_bytes(struct pxprpc_serializer *ser,uint8_t *val,uint32_t size){
    if(size<=0xff){
        _pxprpc_ensure_buf(ser,1+size);
        *(ser->buf+ser->pos)=size;
        ser->pos++;
    }else{
        _pxprpc_ensure_buf(ser,5+size);
        *(ser->buf+ser->pos)=0xff;
        ser->pos++;
        memmove(ser->buf+ser->pos,&size,4);
        ser->pos+=4;
    }
    memcpy(ser->buf+ser->pos,val,size);
    ser->pos+=size;
}
extern void pxprpc_ser_get_int(struct pxprpc_serializer *ser,int32_t *val,int count){
    memmove(val,ser->buf+ser->pos,4*count);
    ser->pos+=4*count;
}
extern void pxprpc_ser_get_long(struct pxprpc_serializer *ser,int64_t *val,int count){
    memmove(val,ser->buf+ser->pos,8*count);
    ser->pos+=8*count;
}
extern void pxprpc_ser_get_float(struct pxprpc_serializer *ser,float *val,int count){
    memmove(val,ser->buf+ser->pos,4*count);
    ser->pos+=4*count;
}
extern double pxprpc_ser_get_double(struct pxprpc_serializer *ser,double *val,int count){
    memmove(val,ser->buf+ser->pos,8*count);
    ser->pos+=8*count;
}
extern uint8_t *pxprpc_ser_get_bytes(struct pxprpc_serializer *ser,uint32_t *size){
    uint32_t len=*(ser->buf+ser->pos);
    ser->pos++;
    if(len==0xff){
        memmove(&len,ser->buf+ser->pos,4);
        ser->pos+=4;
    }
    *size=len;
    uint8_t *val=ser->buf+ser->pos;
    ser->pos+=*size;
    return val;
}