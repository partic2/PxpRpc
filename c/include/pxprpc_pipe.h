#ifndef _PXPRPC_PIPE_H
#define _PXPRPC_PIPE_H

#include <pxprpc.h>


void pxprpc_pipe_serve(const char *name,void (*on_connect)(struct pxprpc_abstract_io *,void *p),void *p);

struct pxprpc_abstract_io *pxprpc_pipe_connect(const char *name);

/* 
pxprpc_pipe API is not thread safe. 
"pxprpc_pipe_executor" can be set by user to to execute function in pxprpc_pipe thread.
Though pxprpc_pipe don't use it internally and set it to NULL by default.
*/
extern void (*pxprpc_pipe_executor)(void (*fn)(void *p),void *p);

/* 
This const string is returned by io->get_error when connection is closed. 
In this case , connection is destructing and don't use the connection further after "onCompleted" returned.
BTW: pxprpc_pipe_error_connection_closed="Connection closed."
*/

extern const char *pxprpc_pipe_error_connection_closed;


#endif