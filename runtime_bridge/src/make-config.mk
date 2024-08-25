

# PXPRPC_SOURCE_DIR=path of this project.

include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk
include $(PXPRPC_SOURCE_DIR)/c/pxprpc_pipe/make-config.mk


PXPRPC_RTBRIDGE_CFLAGS= $(PXPRPC_CFLAGS) $(PXPRPC_PIPE_CFLAGS) -I$(PXPRPC_SOURCE_DIR)/runtime_bridge/include

PXPRPC_RTBRIDGE_LDFLAGS= pxprpc_rtbridge.o $(PXPRPC_LDFLAGS) $(PXPRPC_PIPE_LDFLAGS)

pxprpc_rtbridge_test:test.c build_pxprpc_rtbridge
	$(CC) -o __temp_test.exe $(CFLAGS) $(PXPRPC_RTBRIDGE_CFLAGS) test.c $(PXPRPC_PIPE_LDFLAGS)

build_pxprpc_rtbridge:$(PXPRPC_SOURCE_DIR)/runtime_bridge/src/pxprpc_rtbridge.c build_pxprpc_pipe
	$(CC) -c -o pxprpc_rtbridge.o $(CFLAGS) $(PXPRPC_RTBRIDGE_CFLAGS) $<
