

# PXPRPC_SOURCE_DIR=path of this project.

ifndef PXPRPC_RTBRIDGE_CFLAGS

include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk
include $(PXPRPC_SOURCE_DIR)/c/pxprpc_pipe/make-config.mk


PXPRPC_RTBRIDGE_CFLAGS= $(PXPRPC_CFLAGS) $(PXPRPC_PIPE_CFLAGS) -I$(PXPRPC_SOURCE_DIR)/runtime_bridge/include

PXPRPC_RTBRIDGE_LDFLAGS= pxprpc_rtbridge.o pxprpc_rtbridgepp.o $(PXPRPC_LDFLAGS) $(PXPRPC_PIPE_LDFLAGS) $(LIBUV_LDFLAGS) -lstdc++

pxprpc_rtbridge_test:test.cpp build_pxprpc_rtbridge
	$(CC) -o __temp_test.exe $(CFLAGS) $(PXPRPC_RTBRIDGE_CFLAGS) test.cpp $(PXPRPC_RTBRIDGE_LDFLAGS)

build_pxprpc_rtbridge:$(PXPRPC_SOURCE_DIR)/runtime_bridge/src/pxprpc_rtbridge.c build_pxprpc_pipe build_pxprpc
	$(CC) -c $(CFLAGS) $(PXPRPC_RTBRIDGE_CFLAGS) $(PXPRPC_SOURCE_DIR)/runtime_bridge/src/pxprpc_rtbridge.c $(PXPRPC_SOURCE_DIR)/runtime_bridge/src/pxprpc_rtbridgepp.cpp

endif