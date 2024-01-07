

#include <iostream>

#include <tbox/tbox.h>
extern "C"{
    #include <pxprpc_tbox.h>
}


#include <pxprpc_pp.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <string>

using namespace pxprpc;

pxprpc_tbox_api *srvtbox;

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
        std::cerr<<"tb_socket_bind failed"<<std::endl;
    }

    fnPrintString fn1;
    fn1.setName("printString");
    fnPrintSerilizedArgs fn3;
    fn3.setName("printSerilizedArgs");
    pxprpc_namedfunc namedfns[2]={
        *fn1.cNamedFunc(),*fn3.cNamedFunc()
    };
    auto tbrpc=srvtbox->new_server(sock,namedfns,2);
    srvtbox->serve_block(tbrpc);
    std::cerr<<"serve_block failed"<<srvtbox->get_error()<<std::endl;
    return 0;
}