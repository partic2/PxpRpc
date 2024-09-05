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



/* 
  Initialize rtbridge with libuv.
  Return error string if error occured, or NULL.
  The "uvloop" will be used as "main loop" for rtbridge and can be only set once. 
  The second and later pxprpc_rtbrige_init_uv call before uv_rtbirdge_deinit will take no effect and return "inited".
  This function will modify pxprpc_pipe_executor if pxprpc_pipe_executor is NULL.
  BTW rtbridge will register an async_handle so uv_run(uvloop,UV_RUN_DEFAULT) will never return before pxprpc_rtbridge_deinit is called.
*/
char *pxprpc_rtbridge_init_uv(void *uvloop);

/*
  Initialize rtbridge with a new uv loop and run this loop in new thread.
  return NULL if no error.
  return "inited" if rtbridge has been inited by other loop.
  return error meesage for other error.
*/
char *pxprpc_rtbridge_init_and_run();

/*
  Deinitialize rtbridge. Return error string if error occured, or NULL.
*/
char *pxprpc_rtbridge_deinit();