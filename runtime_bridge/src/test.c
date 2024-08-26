

#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <pxprpc_rtbridge.h>
#include <uv.h>


uv_thread_t thread1;
uv_thread_t thread2;

struct pxprpc_abstract_io *servSideIo=NULL;
struct pxprpc_abstract_io *cliSideIo=NULL;


void thread1Entry(void *){
    while(servSideIo==NULL);
    printf("thread1 start\n");
    struct pxprpc_buffer_part sendbuf;
    sendbuf.bytes.base=(void *)"aabbcc";
    sendbuf.bytes.length=7;
    sendbuf.next_part=NULL;
    pxprpc_rtbridge_bsend(servSideIo,&sendbuf);
}

void thread2Entry(void *){
    while(cliSideIo==NULL);
    printf("thread2 start\n");
    struct pxprpc_buffer_part recvbuf;
    recvbuf.bytes.base=NULL;
    recvbuf.bytes.length=0;
    recvbuf.next_part=NULL;
    pxprpc_rtbridge_brecv(cliSideIo,&recvbuf);
    printf("thread2 received:%s\n",recvbuf.bytes.base);
    cliSideIo->buf_free(recvbuf.bytes.base);
}

void pipeOnConnect(struct pxprpc_abstract_io *io,void *p1){
    servSideIo=io;
}

int main(int argc,char *argv[]){
    printf("pxprpc_rtbridge_test begin\n");
    pxprpc_pipe_serve("test1",pipeOnConnect,NULL);
    cliSideIo=pxprpc_pipe_connect("test1");
    pxprpc_rtbridge_init_uv(uv_default_loop());
    uv_thread_create(&thread1,thread1Entry,NULL);
    uv_thread_create(&thread2,thread2Entry,NULL);
    printf("uv_run\n");
    uv_run(uv_default_loop(),UV_RUN_DEFAULT);
    printf("exit...\n");
    return 0;
}

