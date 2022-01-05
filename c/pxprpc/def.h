
#ifndef _PXPRPC_DEF_H
#define _PXPRPC_DEF_H

#include <stdint.h>

struct pxprpc_abstract_io{
    void (*read)(struct pxprpc_abstract_io *self,uint32_t length,uint8_t *buf,void (*onCompleted)(void *p),void *p);
    void (*write)(struct pxprpc_abstract_io *self,uint32_t length,const uint8_t *buf);
    void *userData;
};

struct pxprpc_bytes{
    uint32_t length;
    //variable length array
    uint8_t data[1];
};

#endif