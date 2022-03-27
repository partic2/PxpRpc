

# PXPRPC_SOURCE_ROOT=path of this project.

C_SOURCE_FILES+= $(PXPRPC_SOURCE_ROOT)/c/pxprpc_libuv/server_libuv.c

CFLAGS+= $(LIBUV_CFLAGS)

LDFLAGS+= $(LIBUV_LDFLAGS)

include $(PXPRPC_SOURCE_ROOT)/c/pxprpc/make-config.mk


pxprpc_libuv_test:$(C_SOURCE_FILES) $(PXPRPC_SOURCE_ROOT)/c/pxprpc_libuv/test.cpp
	$(CC) -o __temp_test -g $(CFLAGS) test.cpp $(C_SOURCE_FILES) $(LDFLAGS) -lstdc++
	./__temp_test