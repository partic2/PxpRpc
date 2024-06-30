

# PXPRPC_SOURCE_DIR=path of this project.


include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk

PXPRPC_PIPE_CFLAGS+= -I$(PXPRPC_SOURCE_DIR)/native_pipe/include $(PXPRPC_CFLAGS)

PXPRPC_PIPE_LDFLAGS+= pxprpc_pipe.o

build_pxprpc_pipe:$(PXPRPC_SOURCE_DIR)/native_pipe/core/pxprpc_pipe.c
	$(CC) -c -o pxprpc_pipe.o $(CFLAGS) $(PXPRPC_PIPE_CFLAGS) $<
