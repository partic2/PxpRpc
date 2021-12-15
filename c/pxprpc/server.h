
#include <stdint.h>

#pragma pack(1)


struct pxprpc_object{
    void *object1;
    uint16_t size_of_struct;
    uint16_t count;
    uint16_t (*addRef)(struct pxprpc_object *ref);
    uint16_t (*release)(struct pxprpc_object *ref);
};
