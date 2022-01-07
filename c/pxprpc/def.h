
#ifndef _PXPRPC_DEF_H
#define _PXPRPC_DEF_H

#include <stdint.h>

struct pxprpc_abstract_io{
    void (*read)(struct pxprpc_abstract_io *self,uint32_t length,uint8_t *buf,void (*onCompleted)(void *args),void *p);
    void (*write)(struct pxprpc_abstract_io *self,uint32_t length,const uint8_t *buf);
    //get last error. For example, to get error caused by read, call "io->get_error(io,io->read)". return NULL if no error
    const char *(*get_error)(struct pxprpc_abstract_io *self,void *fn);
    void *userData;
};

struct pxprpc_bytes{
    uint32_t length;
    //variable length array
    uint8_t data[1];
};

#endif