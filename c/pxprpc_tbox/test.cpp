

#include <iostream>

#include <tbox/tbox.h>
extern "C"{
    #include "server_tbox.h"
}


#include "../pxprpc/cppwrap/server_pp.hpp"
#include "functional"
#include <memory>


pxprpc_tbox_api *srvtbox;

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



int main(int argc,char *argv[]){
    pxprpc::init();
    pxprpc_tbox_query_interface(&srvtbox);
    if(tb_init(tb_null,tb_null)==tb_false){
        std::cerr<<"tb_init failed"<<errno<<std::endl;
    };
    tb_socket_ref_t sock=tb_socket_init(TB_SOCKET_TYPE_TCP,TB_IPADDR_FAMILY_IPV4);
    tb_ipaddr_t ipaddr;
    tb_ipaddr_clear(&ipaddr);
    if(tb_ipaddr_set(&ipaddr,"127.0.0.1",1089,TB_IPADDR_FAMILY_IPV4)==tb_false){
        std::cerr<<"tb_ipaddr_set failed"<<std::endl;
    }
    if(tb_socket_bind(sock,&ipaddr)==tb_false){
        int err=WSAGetLastError();
        std::cerr<<"tb_socket_bind failed"<<err<<std::endl;
    }
    fnPrintString fn1(std::string("printString"));
    fnPrintStringUnderline fn2(std::string("printStringUnderline"));
    pxprpc_namedfunc namedfns[2]={
        *fn2.cNamedFunc(),*fn1.cNamedFunc()
    };
    auto tbrpc=srvtbox->new_server(sock,namedfns,2);
    srvtbox->serve_block(tbrpc);
    std::cerr<<"serve_block failed"<<srvtbox->get_error()<<std::endl;
    return 0;
}