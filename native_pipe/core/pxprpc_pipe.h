#ifndef _PXPRPC_PIPE_H
#define _PXPRPC_PIPE_H

#include <pxprpc.h>

void pxprpc_pipe_serve(const char *name,void (*on_connect)(struct pxprpc_abstract_io *));

struct pxprpc_abstract_io *pxprpc_pipe_connect(const char *name);

void pxprpc_pipe_close(struct pxprpc_abstract_io *io);


#endif