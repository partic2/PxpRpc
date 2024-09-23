#pragma once

#include <pxprpc_pp.hpp>
#include <queue>
#include <functional>
extern "C"{
    #include <pxprpc_pipe.h>
}

#include <iostream>

namespace pxprpc_rtbridge_base{
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
    class PxpConnection:public pxprpc::PxpObject{
        public:
        pxprpc_abstract_io *io;
        PxpConnection(pxprpc_abstract_io *io_in):io(io_in){
        }
        virtual ~PxpConnection(){
            io->close(io);
        }
    };
    void __wrap_on_connect(struct pxprpc_abstract_io *io1,void *server){
        auto serv=reinterpret_cast<PxpPipeServer *>(server);
        serv->onConnect(io1);
    }
    
    bool inited=false;
    void init(){
        if(!inited){
            defaultFuncMap.add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe_pp.serve",
            [](auto para,auto ret)->void{
                auto obj=new PxpPipeServer(para->nextString());
                obj->serve();
                ret->resolve(obj);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe_pp.accept",
            [](auto para,auto ret)->void{
                auto serv=static_cast<PxpPipeServer *>(para->nextObject());
                serv->accept([ret](auto io)->void{
                    ret->resolve(new PxpConnection(io));
                });
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe_pp.connect",
            [](auto para,auto ret)->void{
                auto servName=para->nextString();
                ret->resolve(new PxpConnection(pxprpc_pipe_connect(servName.c_str())));
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_to_raw_addr",
            [](auto para,auto ret)->void{
                auto ioaddr=reinterpret_cast<int64_t>(static_cast<PxpConnection *>((para->nextObject()))->io);
                ret->resolve(ioaddr);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_from_raw_addr",
            [](auto para,auto ret)->void{
                ret->resolve(new PxpConnection(reinterpret_cast<struct pxprpc_abstract_io *>(para->nextLong())));
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_send",
            [](auto para,auto ret)->void{
                auto ioaddr=static_cast<PxpConnection *>((para->nextObject()))->io;
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
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_receive",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para,auto ret)->void{
                auto ioaddr=static_cast<PxpConnection *>((para->nextObject()))->io;
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
