

#include <iostream>

extern "C"{
    #include "server_libuv.h"
}


#include "../pxprpc/cppwrap/server_pp.hpp"
#include "functional"
#include <memory>


pxprpc_libuv_api *srvtbox;

uint8_t testFnWriteBuf[4];
static void testNopCallback(void *p){};

class fnPrintString:public pxprpc::NamedFunctionPP{
    public:
    fnPrintString(std::string funcName):NamedFunctionPP(funcName){};
    virtual void readParameter(struct pxprpc_request *r,std::function<void()> doneCallback){
        auto buf=std::make_shared<std::array<uint8_t,4>>();
        this->readFromIo(r->io1,buf->data(),4,[doneCallback,buf,r](pxprpc_abstract_io *io1,const char *error)->void{
            if(error){
                std::cout<<"get error:"<<error<<std::endl;
                doneCallback();
            }
            auto addr=*reinterpret_cast<uint32_t *>(buf->data());
            r->callable_data=r->ref_slots[addr]->object1;
            doneCallback();
        });
        
    };
    virtual void call(struct pxprpc_request *r,std::function<void(pxprpc::RpcObject *)> onResult){
        auto str1=reinterpret_cast<char *>(&static_cast<pxprpc_bytes *>(r->callable_data)->data);
        auto len=static_cast<pxprpc_bytes *>(r->callable_data)->length;
        if(str1[len-1]!=0){
            str1[len-1]=0;
        }
        std::cout<<str1<<std::endl;
        onResult(new pxprpc::RpcRawBytes((uint8_t *)"hello client",strlen("hello client")));
    };
    virtual void writeResult(struct pxprpc_request *r){
        *(uint32_t *)(&testFnWriteBuf)=r->dest_addr;
        r->io1->write(r->io1,4,testFnWriteBuf,testNopCallback,NULL);
    };
};

class fnPrintStringUnderline:public pxprpc::NamedFunctionPP{
    public:
    fnPrintStringUnderline(std::string funcName):NamedFunctionPP(funcName){};
    virtual void readParameter(struct pxprpc_request *r,std::function<void()> doneCallback){
        auto buf=std::make_shared<std::array<uint8_t,4>>();
        this->readFromIo(r->io1,buf->data(),4,[doneCallback,buf,r](pxprpc_abstract_io *io1,const char *error)->void{
            if(error){
                std::cout<<"get error:"<<error<<std::endl;
                doneCallback();
            }
            auto addr=*reinterpret_cast<uint32_t *>(buf->data());
            r->callable_data=r->ref_slots[addr]->object1;
            doneCallback();
        });
        
    };
    virtual void call(struct pxprpc_request *r,std::function<void(pxprpc::RpcObject *)> onResult){
        auto str1=reinterpret_cast<char *>(&static_cast<pxprpc_bytes *>(r->callable_data)->data);
        auto len=static_cast<pxprpc_bytes *>(r->callable_data)->length;
        if(str1[len-1]!=0){
            str1[len-1]=0;
        }
        std::cout<<"__"<<str1<<std::endl;
        onResult(new pxprpc::RpcRawBytes((uint8_t *)"hello client",strlen("hello client")));
    };
    virtual void writeResult(struct pxprpc_request *r){
        *(uint32_t *)(&testFnWriteBuf)=r->dest_addr;
        r->io1->write(r->io1,4,testFnWriteBuf,testNopCallback,NULL);
    };
};

uv_tcp_t servTcp;

int main(int argc,char *argv[]){
    pxprpc::init();
    pxprpc_libuv_query_interface(&srvtbox);
    uv_loop_t *loop=uv_default_loop();
    
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
    fnPrintString fn1(std::string("printString"));
    fnPrintStringUnderline fn2(std::string("printStringUnderline"));
    pxprpc_namedfunc namedfns[2]={
        *fn2.cNamedFunc(),*fn1.cNamedFunc()
    };
    auto tbrpc=srvtbox->new_server(loop,(uv_stream_t *)&servTcp,namedfns,2);
    srvtbox->serve_start(tbrpc);
    uv_run(loop,UV_RUN_DEFAULT);
    printf("libuv finish.");
    return 0;
}