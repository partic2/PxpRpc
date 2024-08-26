

# PXPRPC_SOURCE_DIR=path of this project.

ifndef PXPRPC_PIPE_CFLAGS

include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk

PXPRPC_PIPE_CFLAGS= $(PXPRPC_CFLAGS)

PXPRPC_PIPE_LDFLAGS= pxprpc_pipe.o

pxprpc_pipe_test:test.c build_pxprpc_pipe
	$(CC) -o __temp_test.exe $(CFLAGS) $(PXPRPC_PIPE_CFLAGS) test.c $(PXPRPC_PIPE_LDFLAGS)

build_pxprpc_pipe:$(PXPRPC_SOURCE_DIR)/c/pxprpc_pipe/pxprpc_pipe.c
	$(CC) -c -o pxprpc_pipe.o $(CFLAGS) $(PXPRPC_PIPE_CFLAGS) $<


endif