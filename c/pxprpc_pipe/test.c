

#include <pxprpc.h>
#include <pxprpc_pipe.h>
#include <stdio.h>



struct pxprpc_buffer_part buff;

void serverReceiveDone(void *p);

void serverSendDone(void *p){
    puts("serverSendDone\n");
    struct pxprpc_abstract_io *io1=(struct pxprpc_abstract_io *)p;
    if(io1->get_error(io1,io1->send)!=NULL){
        if(io1->get_error(io1,io1->send)==pxprpc_pipe_error_connection_closed){
            printf("closed");
            io1->close(io1);
            return;
        }else{
            printf("error occured:%s,%d",io1->get_error(io1,io1->send),__LINE__);
            exit(1);
        }
    }
    io1->buf_free(buff.bytes.base);
    io1->receive(io1,&buff,&serverReceiveDone,io1);
}

void serverReceiveDone(void *p){
    puts("serverReceiveDone\n");
    struct pxprpc_abstract_io *io1=(struct pxprpc_abstract_io *)p;
    if(io1->get_error(io1,io1->receive)!=NULL){
        if(io1->get_error(io1,io1->receive)==pxprpc_pipe_error_connection_closed){
            printf("closed");
            io1->close(io1);
            return;
        }else{
            printf("error occured:%s,%d",io1->get_error(io1,io1->receive),__LINE__);
            exit(1);
        }
    }
    io1->send(io1,&buff,serverSendDone,io1);
}

void testEchoServer1OnConnect(struct pxprpc_abstract_io *io1,void *p){
    puts("testEchoServer1OnConnect\n");
    buff.bytes.base=0;
    buff.bytes.length=0;
    buff.next_part=NULL;
    io1->receive(io1,&buff,&serverReceiveDone,io1);
}

struct pxprpc_buffer_part clientBuff1;
struct pxprpc_buffer_part clientBuff2;

void testClientRecvDone(void *p){
    puts("testClientRecvDone\n");
    struct pxprpc_abstract_io *io1=(struct pxprpc_abstract_io *)p;
    if(io1->get_error(io1,io1->receive)!=NULL){
        if(io1->get_error(io1,io1->receive)==pxprpc_pipe_error_connection_closed){
            printf("closed");
            io1->close(io1);
            return;
        }else{
            printf("error occured:%s,%d",io1->get_error(io1,io1->receive),__LINE__);
            exit(1);
        }
    }
    puts("received");
    fwrite(clientBuff1.bytes.base,1,4,stdout);
    puts("\n");
    fwrite(clientBuff2.bytes.base,1,clientBuff2.bytes.length,stdout);
    puts("\n");
}

void testClientSendDone(void *p){
    puts("testClientSendDone\n");
    struct pxprpc_abstract_io *io1=(struct pxprpc_abstract_io *)p;
    if(io1->get_error(io1,io1->send)!=NULL){
        if(io1->get_error(io1,io1->send)==pxprpc_pipe_error_connection_closed){
            printf("closed");
            io1->close(io1);
            return;
        }else{
            printf("error occured:%s,%d",io1->get_error(io1,io1->send),__LINE__);
            exit(1);
        }
    }
    clientBuff1.bytes.length=4;
    free(clientBuff2.bytes.base);
    clientBuff2.bytes.length=0;
    clientBuff2.bytes.base=NULL;
    io1->receive(io1,&clientBuff1,testClientRecvDone,io1);
    io1->close(io1);
}

void test1(){
    pxprpc_pipe_serve("testEchoServer1",testEchoServer1OnConnect,NULL);
    struct pxprpc_abstract_io *client=pxprpc_pipe_connect("testEchoServer1");
    clientBuff1.bytes.base=malloc(10);
    clientBuff1.bytes.length=5;
    sprintf((char *)clientBuff1.bytes.base,"12345");
    clientBuff1.next_part=&clientBuff2;
    clientBuff2.bytes.base=malloc(10);
    clientBuff2.bytes.length=5;
    clientBuff2.next_part=NULL;
    sprintf((char *)clientBuff2.bytes.base,"67890");
    client->send(client,&clientBuff1,testClientSendDone,client);
}

int main(int argc,char *argv[]){
    puts("main\n");
    test1();
    return 0;
}