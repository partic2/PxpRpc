

# PXPRPC_SOURCE_ROOT=path of this project.

C_SOURCE_FILES+= $(PXPRPC_SOURCE_ROOT)/c/pxprpc/server.c

CFLAGS+= -I$(PXPRPC_SOURCE_ROOT)/c
