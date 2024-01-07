

#include <iostream>

extern "C"{
    #include <pxprpc_libuv.h>
}


#include <pxprpc_pp.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <string>


pxprpc_libuv_api *srvtbox;
uv_tcp_t servTcp;
uv_loop_t *loop;

uint8_t testFnWriteBuf[4];
const int constZero=0;
static void testNopCallback(void *p){};

using namespace pxprpc;

class fnPrintString:public FunctionPPWithSerializer{
    public:
    virtual void callWithSer(PxpRequestWrap *r,Serializer *parameter,std::function<void(Serializer *result)> done){
        std::cout<<parameter->getString()<<std::endl;
        done((new Serializer())->prepareSerializing(32)->putString("hello client"));
    }
};

class fnPrintSerilizedArgs:public FunctionPPWithSerializer{
    public:
    virtual void callWithSer(PxpRequestWrap *r,Serializer *parameter,std::function<void(Serializer *result)> done){
        auto i=parameter->getInt();
        auto l=parameter->getLong();
        auto f=parameter->getFloat();
        auto d=parameter->getDouble();
        auto s=parameter->getString();
        auto b=parameter->getString();
        std::cout<<i<<","<<l<<","<<f<<","<<d<<","<<s<<","<<b<<std::endl;
        done(nullptr);
    }
};



int main(int argc,char *argv[]){
    pxprpc::init();
    pxprpc_libuv_query_interface(&srvtbox);
    loop=uv_default_loop();
    
    memset(&servTcp,0,sizeof(uv_tcp_t));
    uv_tcp_init(loop,&servTcp);
    struct sockaddr_in saddr;
    int r=uv_ip4_addr("127.0.0.1",1089,&saddr);
    if(r){
        printf("uv_ip4_addr failed.");
    }
    r=uv_tcp_bind(&servTcp,(const sockaddr *)&saddr,0);
    if(r){
        printf("uv_tcp_bind failed.");
    }

    fnPrintString fn1;
    fn1.setName("printString");
    fnPrintSerilizedArgs fn3;
    fn3.setName("printSerilizedArgs");
    pxprpc_namedfunc namedfns[2]={
        *fn1.cNamedFunc(),*fn3.cNamedFunc()
    };
    auto tbrpc=srvtbox->new_server(loop,(uv_stream_t *)&servTcp,namedfns,2);
    srvtbox->serve_start(tbrpc);
    uv_run(loop,UV_RUN_DEFAULT);
    printf("libuv finish.");
    return 0;
}