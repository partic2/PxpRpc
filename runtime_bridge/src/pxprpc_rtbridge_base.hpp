

#include <pxprpc_pp.hpp>
#include <queue>
#include <functional>
extern "C"{
    #include <pxprpc_pipe.h>
}

#include <iostream>

namespace pxprpc_rtbirdge_base{
    void __callArg0Function(void *cb){
        auto cb2=reinterpret_cast<std::function<void()>*>(cb);
        (*cb2)();
        delete cb2;
    }  
    void __wrap_on_connect(struct pxprpc_abstract_io *io1,void *server);
    using namespace pxprpc;
    class PxpPipeServer : public PxpObject{
        public:
        std::string s;
        std::function<void(struct pxprpc_abstract_io *)> newConn;
        std::queue<struct pxprpc_abstract_io *> connected;
        bool waiting=false;
        PxpPipeServer(std::string s){
            this->s=s;
        }
        virtual void onConnect(struct pxprpc_abstract_io *io1){
            if(waiting){
                waiting=false;
                newConn(io1);
            }else{
                connected.push(io1);
            }
        }
        void serve(){
            pxprpc_pipe_serve(s.c_str(),&__wrap_on_connect,this);
        }
        void accept(std::function<void(struct pxprpc_abstract_io *)> newConn){
            if(connected.empty()){
                this->newConn=newConn;
                waiting=true;
            }else{
                newConn(connected.front());
                connected.pop();
            }
        }
        virtual ~PxpPipeServer(){
            pxprpc_pipe_serve(s.c_str(),nullptr,nullptr);
        }
    };
    void __wrap_on_connect(struct pxprpc_abstract_io *io1,void *server){
        auto serv=reinterpret_cast<PxpPipeServer *>(server);
        serv->onConnect(io1);
    }
    
    bool inited=false;
    void init(){
        if(!inited){
            defaultFuncMap.add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe.serve",
            [](auto para,auto ret)->void{
                auto obj=new PxpPipeServer(para->nextString());
                obj->serve();
                ret->resolve(obj);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe.accept",
            [](auto para,auto ret)->void{
                auto serv=static_cast<PxpPipeServer *>(para->nextObject());
                serv->accept([ret](auto io)->void{
                    auto ioi64=reinterpret_cast<int64_t>(io);
                    ret->resolve(ioi64);
                });
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe.connect",
            [](auto para,auto ret)->void{
                auto servName=para->nextString();
                auto conn=reinterpret_cast<int64_t>(pxprpc_pipe_connect(servName.c_str()));
                ret->resolve(conn);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc.io_send",
            [](auto para,auto ret)->void{
                auto ioaddr=reinterpret_cast<struct pxprpc_abstract_io *>(para->nextLong());
                auto buffer1=new pxprpc_buffer_part();
                auto p2=para->nextBytes();
                buffer1->bytes.length=std::get<0>(p2);
                buffer1->bytes.base=std::get<1>(p2);
                buffer1->next_part=nullptr;
                auto cb=new std::function<void()>([ret,buffer1,ioaddr]()->void{
                    auto err=ioaddr->get_error(ioaddr,reinterpret_cast<void *>(ioaddr->send));
                    if(err!=nullptr){
                        ret->reject(err);
                    }else{
                        ret->resolve();
                    }
                    delete buffer1;
                });
                ioaddr->send(ioaddr,buffer1,__callArg0Function,cb);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc.io_receive",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para,auto ret)->void{
                auto ioaddr=reinterpret_cast<struct pxprpc_abstract_io *>(para->nextLong());
                auto buffer1=new pxprpc_buffer_part();
                buffer1->bytes.base=nullptr;
                buffer1->bytes.length=0;
                buffer1->next_part=nullptr;
                auto cb=new std::function<void()>([ret,buffer1,ioaddr]()->void{
                    auto err=ioaddr->get_error(ioaddr,reinterpret_cast<void *>(ioaddr->receive));
                    if(err!=nullptr){
                        ret->reject(err);
                    }else{
                        ret->resolve(*buffer1,[ioaddr,buffer1]()->void{
                            ioaddr->buf_free(buffer1->bytes.base);
                        });
                    }
                    delete buffer1;
                });
                ioaddr->receive(ioaddr,buffer1,__callArg0Function,cb);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc.io_close",
            [](auto para,auto ret)->void{
                auto ioaddr=reinterpret_cast<struct pxprpc_abstract_io *>(para->nextLong());
                ioaddr->close(ioaddr);
                ret->resolve();
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge_host.new_tcp_rpc_server",
            [](auto para,auto ret)->void{
                auto ser=para->asSerializer();
                auto host=ser->getString();
                auto port=ser->getInt();
                auto tcp=new pxprpc_rtbridge_host::TcpPxpRpcServer(host,port);
                ret->resolve(tcp);
            }));
        }
    }
}
