

# PXPRPC_SOURCE_DIR=path of this project.

ifndef PXPRPC_CFLAGS

PXPRPC_CFLAGS= -I$(PXPRPC_SOURCE_DIR)/c/include

PXPRPC_LDFLAGS= pxprpc.o

build_pxprpc:$(PXPRPC_SOURCE_DIR)/c/pxprpc/pxprpc.c
	$(CC) -c -o pxprpc.o $(CFLAGS) $(PXPRPC_CFLAGS) $< $(LDFLAGS)

endif