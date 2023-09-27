

# PXPRPC_SOURCE_DIR=path of this project.

PXPRPC_CFLAGS+= -I$(PXPRPC_SOURCE_DIR)/c

PXPRPC_LDFLAGS+= pxprpc_server.o

build_pxprpc:$(PXPRPC_SOURCE_DIR)/c/pxprpc/server.c
	$(CC) -c -o pxprpc_server.o $(CFLAGS) $< $(LDFLAGS)
