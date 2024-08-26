

# PXPRPC_SOURCE_DIR=path of this project.

ifndef PXPRPC_TBOX_CFLAGS

include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk

PXPRPC_TBOX_CFLAGS=$(PXPRPC_CFLAGS) $(TBOX_CFLAGS)

PXPRPC_TBOX_LDFLAGS=pxprpc_tbox.o $(PXPRPC_LDFLAGS) $(TBOX_LDFLAGS)



pxprpc_tbox_test:build_pxprpc_tbox $(PXPRPC_SOURCE_DIR)/c/pxprpc_tbox/test.cpp
	$(CC) -o __temp_test -g $(CFLAGS) $(PXPRPC_TBOX_CFLAGS) test.cpp $(LDFLAGS) $(PXPRPC_TBOX_LDFLAGS) -lstdc++
	./__temp_test

build_pxprpc_tbox:$(PXPRPC_SOURCE_DIR)/c/pxprpc_tbox/pxprpc_tbox.c build_pxprpc
	$(CC) -o pxprpc_tbox.o -c $(CFLAGS) $(PXPRPC_CFLAGS) $<

endif