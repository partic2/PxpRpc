

## Use pxprpc with Makefile

Set environment variable "PXPRPC_SOURCE_ROOT" to the path of this project.

In makefile, add make config script by "include", like below

`include $(PXPRPC_SOURCE_ROOT)/c/pxprpc/make-config.mk`

or 

`include $(PXPRPC_SOURCE_ROOT)/c/pxprpc_libuv/make-config.mk`



These scripts will add the source files list into variable `$(C_SOURCE_FILES)`, and also other build flags into `$(CFLAGS)`, `$(LDFLAGS).`
then build like below

`$(CC) -o myprog $(CFLAGS) myprog.cpp $(C_SOURCE_FILES) $(LDFLAGS)`



See also pxprpc_libuv/make-config.mk, which contains a build example.


You may need set `$(LIBUV_CFLAGS)` and `$(LIBUV_LDFLAGS)` variable if use pxprpc_libuv backend. 


Also `$(TBOX_CFLAGS)` and `$(TBOX_LDFLAGS)` variable if use pxprpc_tbox backend. 

