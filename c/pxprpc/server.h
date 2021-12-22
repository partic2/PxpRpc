
#include <stdint.h>

#include "def.h"

#pragma pack(1)

struct pxprpc_object{
    void *object1;
    uint16_t size_of_struct;
    uint16_t count;
    uint16_t (*addRef)(struct pxprpc_object *ref);
    uint16_t (*release)(struct pxprpc_object *ref);
};

struct pxprpc_request{
    void *context;
    struct pxprpc_abstract_io *io1;
    struct pxprpc_object **refSlots;
};


struct pxprpc_callable{
    void (*readParameter)(struct pxprpc_callable *self,struct pxprpc_request *r);
    void (*call)(struct pxprpc_request r,void(*onResult)(struct pxprpc_request r,struct pxprpc_object *result));
    void (*writeResult)(struct pxprpc_callable *self,struct pxprpc_request *r);
};