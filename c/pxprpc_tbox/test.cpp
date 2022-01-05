

#include <iostream>

#include <tbox/tbox.h>
#include "server_tbox.h"

#include "../pxprpc/cppwrap/server_pp.hpp"
#include "functional"


pxprpc_server_tbox_api *srvtbox;


class fnPrintString:pxprpc::NamedFunctionPP{
    public:
    virtual void readParameter(struct pxprpc_request *r,std::function<void()> doneCallback){
        uint8_t buf[4];
        this->readFromIo(r->io1,buf,4,[doneCallback](pxprpc_abstract_io *io1)->void{doneCallback();});
        auto addr=*reinterpret_cast<uint32_t *>(&buf);
        r->callable_data=r->ref_slots[addr]->object1;
    };
    virtual void call(struct pxprpc_request *r,std::function<void(pxprpc::RpcObject *)> onResult){
        std::cout<<reinterpret_cast<char *>(&static_cast<pxprpc_bytes *>(r->callable_data)->data)<<std::endl;
        onResult(new pxprpc::RpcObject());
    };
    virtual void writeResult(struct pxprpc_request *r){
        uint32_t t=0;
        r->io1->write(r->io1,4,reinterpret_cast<uint8_t *>(&t));
    };
};


int main(int argc,char *argv[]){
    pxprpc_tbox_query_interface(&srvtbox);
    tb_socket_ref_t sock=tb_socket_init(TB_SOCKET_TYPE_IPPROTO_TCP,TB_IPADDR_FAMILY_IPV4);
    tb_ipaddr_ref_t ipaddr;
    tb_ipaddr_set(ipaddr,"0.0.0.0",1033,TB_IPADDR_FAMILY_IPV4);
    tb_socket_bind(sock,ipaddr);
}