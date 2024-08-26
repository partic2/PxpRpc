

# PXPRPC_SOURCE_DIR=path of this project.

ifndef PXPRPC_LIBUV_CFLAGS

include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk

PXPRPC_LIBUV_CFLAGS=$(PXPRPC_CFLAGS) $(LIBUV_CFLAGS)

PXPRPC_LIBUV_LDFLAGS=pxprpc_libuv.o $(PXPRPC_LDFLAGS) $(LIBUV_LDFLAGS)



pxprpc_libuv_test:build_pxprpc_libuv $(PXPRPC_SOURCE_DIR)/c/pxprpc_libuv/test.cpp
	$(CC) -o __temp_test -g $(CFLAGS) $(PXPRPC_LIBUV_CFLAGS) test.cpp $(LDFLAGS) $(PXPRPC_LIBUV_LDFLAGS) -lstdc++
	./__temp_test

build_pxprpc_libuv:$(PXPRPC_SOURCE_DIR)/c/pxprpc_libuv/pxprpc_libuv.c build_pxprpc
	$(CC) -o pxprpc_libuv.o -c $(CFLAGS) $(PXPRPC_CFLAGS) $<

endif