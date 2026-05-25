#ifndef _PXPRPC_RTBRIDGE_H
#define _PXPRPC_RTBRIDGE_H

/* 
  pxprpc runtime bridge is mainly about communicate between runtime in one process. 
*/

#include <stdint.h>
#include <pxprpc.h>



/* thread safe block receive wrap for pxprpc_abstract_io. for some block api. */
const char *pxprpc_rtbridge_brecv(struct pxprpc_abstract_io *io,struct pxprpc_bytes *buf);

/* thread safe block send wrap for pxprpc_abstract_io. for some block api. */
const char *pxprpc_rtbridge_bsend(struct pxprpc_abstract_io *io,struct pxprpc_buffer_part *buf);

/*
    Wrap a pxprpc_abstract_io to make it's method thread-safe.
    param post_executable: post a executable to the thread which to handle the send/receive callback.
    .close will also close the wrapped pxprpc_abstract_io, So don't close again.
*/
struct pxprpc_abstract_io *pxprpc_rtbridge_wrap_thread(struct pxprpc_abstract_io *io,void (*post_executable)(void *p,void (*executable)(void *p2),void *p2),void *p);

/* thread safe pipe connect */
struct pxprpc_abstract_io *pxprpc_rtbridge_pipe_connect(const char *servname);

/* thread safe runtime bridge pipe/stream close */
void pxprpc_rtbridge_io_close(struct pxprpc_abstract_io *io);


void pxprpc_rtbridge_post_executable(void *uvthread,void (*fn)(void *),void *p);


const char *pxprpc_rtbridge_new_uvthread(void **uvthread_output);

const char *pxprpc_rtbridge_stop_uvthread(void **uvthread_inout);

/* init and start the main uv thread, output the uv loop. */
const char *pxprpc_rtbridge_init_and_run(void **uvloop);
const char *pxprpc_rtbridge_deinit(void);

struct pxprpc_rtbridge_state{
  void *uv_loop;
  void *uv_tid;
  void *uvthread;
};
void pxprpc_rtbridge_get_uvthread_state(void *uvthread,struct pxprpc_rtbridge_state *out);
/* the main uv thread */
void pxprpc_rtbridge_get_current_state(struct pxprpc_rtbridge_state *out);


#endif