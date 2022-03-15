

# PXPRPC_SOURCE_ROOT=path of this project.

C_SOURCE_FILES+= $(PXPRPC_SOURCE_ROOT)/c/pxprpc_tbox/server_tbox.c

CFLAGS+= $(TBOX_CFLAGS)

LDFLAGS+= $(TBOX_LDFLAGS)

include $(PXPRPC_SOURCE_ROOT)/c/pxprpc/make-config.mk


pxprpc_tbox_test:$(C_SOURCE_FILES) $(PXPRPC_SOURCE_ROOT)/c/pxprpc_tbox/test.cpp
	$(CC) -o __temp_test -g $(CFLAGS) test.cpp $(C_SOURCE_FILES) $(LDFLAGS) -lstdc++
	./__temp_test