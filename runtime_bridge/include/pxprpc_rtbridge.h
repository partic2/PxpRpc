/* 
  pxprpc runtime bridge is mainly about communicate between runtime in one process. 
*/

#include <stdint.h>
#include <pxprpc.h>



/* thread safe block receive wrap for pxprpc_abstract_io. for some block api. */
char *pxprpc_rtbridge_brecv(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf);

/* thread safe block send wrap for pxprpc_abstract_io. for some block api. */
char *pxprpc_rtbridge_bsend(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf);

/* thread safe pipe connect */
struct pxprpc_abstract_io *pxprpc_rtbridge_pipe_connect(char *servname);



/* init rtbridge with libuv, set pxprpc_pipe_executor only if uvloop!=NULL &&  pxprpc_pipe_executor!=NULL */
void pxprpc_rtbridge_init_uv(void *uvloop);


