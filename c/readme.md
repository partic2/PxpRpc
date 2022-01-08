

## Use pxprpc in Makefile

set environment variable "PXPRPC_SOURCE_ROOT" to the path of this project.

In makefile, add make config script by "include", like below

include $(PXPRPC_SOURCE_ROOT)/c/pxprpc/make-config.mk

or 

include $(PXPRPC_SOURCE_ROOT)/c/pxprpc_tbox/make-config.mk

these scripts will add the source files list into variable $(C_SOURCE_FILES), and also other build flags into $(CFLAGS), $(LDFLAGS).
then build like below

$(CC) -o myprog $(CFLAGS) myprog.cpp $(C_SOURCE_FILES) $(LDFLAGS)

See alse pxprpc_tbox/make-config.mk, which contains a build target for test.

You may need set $(TBOX_CFLAGS) and $(TBOX_LDFLAGS) variable if use pxprpc_tbox backend. 

