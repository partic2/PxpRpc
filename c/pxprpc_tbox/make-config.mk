

# PXPRPC_SOURCE_ROOT=path of this project.

include $(PXPRPC_SOURCE_ROOT)/c/pxprpc/make-config.mk

PXPRPC_TBOX_CFLAGS=$(PXPRPC_CFLAGS) $(TBOX_CFLAGS)

PXPRPC_TBOX_LDFLAGS=pxprpc_server_tbox.o $(PXPRPC_LDFLAGS) $(TBOX_LDFLAGS)



pxprpc_tbox_test:build_pxprpc_tbox $(PXPRPC_SOURCE_ROOT)/c/pxprpc_tbox/test.cpp
	$(CC) -o __temp_test -g $(CFLAGS) $(PXPRPC_TBOX_CFLAGS) test.cpp $(LDFLAGS) $(PXPRPC_TBOX_LDFLAGS) -lstdc++
	./__temp_test

build_pxprpc_tbox:$(PXPRPC_SOURCE_ROOT)/c/pxprpc_tbox/server_tbox.c build_pxprpc
	$(CC) -o pxprpc_server_tbox.o -c $(CFLAGS) $(PXPRPC_CFLAGS) $<