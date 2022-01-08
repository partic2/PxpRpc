

#include <iostream>

#include <tbox/tbox.h>
extern "C"{
    #include "server_tbox.h"
}


#include "../pxprpc/cppwrap/server_pp.hpp"
#include "functional"
#include <memory>


pxprpc_tbox_api *srvtbox;


class fnPrintString:public pxprpc::NamedFunctionPP{
    public:
    fnPrintString(std::string name):NamedFunctionPP(name){};
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
        std::cout<<reinterpret_cast<char *>(&static_cast<pxprpc_bytes *>(r->callable_data)->data)<<std::endl;
        onResult(new pxprpc::RpcRawBytes((uint8_t *)"hello client",strlen("hello client")));
    };
    virtual void writeResult(struct pxprpc_request *r){
        uint32_t t=r->dest_addr;
        r->io1->write(r->io1,4,reinterpret_cast<uint8_t *>(&t));
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
    pxprpc_namedfunc *namedfns=new pxprpc_namedfunc();
    namedfns[0]=*fn1.cNamedFunc();
    auto tbrpc=srvtbox->new_server(sock,namedfns,1);
    srvtbox->serve_block(tbrpc);
    std::cerr<<"serve_block failed"<<srvtbox->get_error()<<std::endl;
    return 0;
}