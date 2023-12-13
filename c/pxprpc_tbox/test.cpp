

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


pxprpc_tbox_api *srvtbox;

uint8_t testFnWriteBuf[4];
const int constZero=0;
static void testNopCallback(void *p){};

class fnPrintString:public pxprpc::NamedFunctionPP{
    public:
    fnPrintString(std::string funcName):NamedFunctionPP(funcName){};
    virtual void readParameter(pxprpc_request *r,std::function<void()> doneCallback){
        auto buf=std::make_shared<std::vector<uint8_t>>(4);
        auto iopp=new pxprpc::IoPP(r->io1);
        iopp->read(4,buf->data(),[doneCallback,buf,r](pxprpc::IoPP *self,const char *error)->void{
            if(error){
                std::cout<<"get error:"<<error<<std::endl;
                doneCallback();
            }
            auto addr=*reinterpret_cast<uint32_t *>(buf->data());
            r->callable_data=pxprpc::RpcRawBytes::at(r->server_context,addr);
            doneCallback();
            delete self;
        });
        
    };
    virtual void call(pxprpc_request *r,std::function<void(pxprpc::RpcObject *)> onResult){
        auto str1=static_cast<pxprpc::RpcRawBytes *>(r->callable_data);
        std::cout<<str1->asString()<<std::endl;
        delete str1;
        onResult(new pxprpc::RpcRawBytes((uint8_t *)"hello client",strlen("hello client")));
    };
    virtual void writeResult(pxprpc_request *r,std::function<void()> doneCallback){
        auto iopp=new pxprpc::IoPP(r->io1);
        iopp->write(4,reinterpret_cast<const uint8_t *>(&constZero),[doneCallback](pxprpc::IoPP *self,const char *error)->void{
            doneCallback();
        });
    };
};

class fnPrintStringUnderline:public pxprpc::NamedFunctionPP{
    public:
    fnPrintStringUnderline(std::string funcName):NamedFunctionPP(funcName){};
    virtual void readParameter(pxprpc_request *r,std::function<void()> doneCallback){
        auto buf=std::make_shared<std::vector<uint8_t>>(4);
        auto iopp=new pxprpc::IoPP(r->io1);
        iopp->read(4,buf->data(),[doneCallback,buf,r](pxprpc::IoPP *self,const char *error)->void{
            if(error){
                std::cout<<"get error:"<<error<<std::endl;
                doneCallback();
            }
            auto addr=*reinterpret_cast<uint32_t *>(buf->data());
            r->callable_data=pxprpc::RpcRawBytes::at(r->server_context,addr);
            doneCallback();
            delete self;
        });
    };
    virtual void call(pxprpc_request *r,std::function<void(pxprpc::RpcObject *)> onResult){
        auto str1=static_cast<pxprpc::RpcRawBytes *>(r->callable_data);
        std::cout<<"__"<<str1->asString()<<std::endl;
        delete str1;
        onResult(new pxprpc::RpcRawBytes((uint8_t *)"hello client",strlen("hello client")));
    };
    virtual void writeResult(pxprpc_request *r,std::function<void()> doneCallback){
        auto iopp=new pxprpc::IoPP(r->io1);
        iopp->write(4,reinterpret_cast<const uint8_t *>(&constZero),[doneCallback](pxprpc::IoPP *self,const char *error)->void{
            doneCallback();
        });
    };
};

class fnPrintSerilizedArgs:public pxprpc::NamedFunctionPP{
    public:
    fnPrintSerilizedArgs(std::string funcName):NamedFunctionPP(funcName){};
    virtual void readParameter(pxprpc_request *r,std::function<void()> doneCallback){
        auto buf=std::make_shared<std::vector<uint8_t>>(4);
        auto iopp=new pxprpc::IoPP(r->io1);
        iopp->read(4,buf->data(),[doneCallback,buf,r](pxprpc::IoPP *self,const char *error)->void{
            if(error){
                std::cout<<"get error:"<<error<<std::endl;
                doneCallback();
            }
            auto addr=*reinterpret_cast<uint32_t *>(buf->data());
            r->callable_data=pxprpc::RpcRawBytes::at(r->server_context,addr);
            doneCallback();
            delete self;
        });
    };
    virtual void call(pxprpc_request *r,std::function<void(pxprpc::RpcObject *)> onResult){
        pxprpc::Serializer ser;
        auto arg0=static_cast<pxprpc::RpcRawBytes *>(r->callable_data);
        ser.prepareUnserializing(arg0->data,arg0->size);
        std::cout<<ser.getInt()<<","<<ser.getLong()<<","<<ser.getFloat()<<","<<ser.getDouble()<<",";
        auto str1=ser.getBytes();
        std::cout<<std::string(reinterpret_cast<char *>(std::get<1>(str1)),std::get<0>(str1))<<",";
        str1=ser.getBytes();
        std::cout<<std::string(reinterpret_cast<char *>(std::get<1>(str1)),std::get<0>(str1))<<",";
        onResult(nullptr);
    };
    virtual void writeResult(pxprpc_request *r,std::function<void()> doneCallback){
        auto iopp=new pxprpc::IoPP(r->io1);
        iopp->write(4,reinterpret_cast<const uint8_t *>(&constZero),[doneCallback](pxprpc::IoPP *self,const char *error)->void{
            doneCallback();
        });
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
        std::cerr<<"tb_socket_bind failed"<<std::endl;
    }

    fnPrintString fn1(std::string("printString"));
    fnPrintStringUnderline fn2(std::string("printStringUnderline"));
    fnPrintSerilizedArgs fn3(std::string("printSerilizedArgs"));
    pxprpc_namedfunc namedfns[3]={
        *fn2.cNamedFunc(),*fn1.cNamedFunc(),*fn3.cNamedFunc()
    };
    auto tbrpc=srvtbox->new_server(sock,namedfns,3);
    srvtbox->serve_block(tbrpc);
    std::cerr<<"serve_block failed"<<srvtbox->get_error()<<std::endl;
    return 0;
}