
#pragma once

#include <pxprpc_pp.hpp>
#include <pxprpc_ext.hpp>
#include <queue>
#include <functional>
#include <pxprpc_rtbridge_host.hpp>

extern "C" {
    #include <pxprpc_pipe.h>
    #include <memfile.h>
}

namespace pxprpc_rtbridge_base{
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
            newConn(nullptr);
        }
    };
    class PxpConnection:public pxprpc::PxpObject{
        public:
        pxprpc_abstract_io *io;
        bool autoClose=true;
        PxpConnection(pxprpc_abstract_io *io_in):io(io_in){
        }
        virtual void setAutoClose(bool autoClose){
            this->autoClose=autoClose;
        }
        virtual ~PxpConnection(){
            if(this->autoClose){
                io->close(io);
            }
        }
    };
    void __wrap_on_connect(struct pxprpc_abstract_io *io1,void *server){
        auto serv=reinterpret_cast<PxpPipeServer *>(server);
        serv->onConnect(io1);
    }
    class MemoryChunk:public pxprpc::PxpObject{
        public:
        //1-malloc
        //2-access
        //3-file mapping
        int createBy=0;
        memfile_t mf={NULL,NULL};
        int32_t size=0;;
        void *base=nullptr;
        const char *alloc(int32_t sizein){
            size=sizein;
            base=::malloc(size);
            createBy=1;
            return nullptr;
        }
        const char *access(void *basein,int32_t sizein){
            base=basein;
            size=sizein;
            createBy=2;
            return nullptr;
        }
        const char* filemap(std::string& path,std::string& mode,int32_t sizein){
            size=sizein;
            int nmode=0;
            if(mode.find('r')!=std::string::npos){
                nmode|=MEMFILE_READ;
            }
            if(mode.find('w')!=std::string::npos){
                nmode|=MEMFILE_WRITE;
            }
            const char* err=memfile_create(path.c_str(),nmode,size,&mf);
            base=mf.data;
            if(err==nullptr)createBy=3;
            return err;
        }
        virtual ~MemoryChunk(){
            switch(createBy){
                case 1:
                ::free(base);
                break;
                case 3:
                memfile_close(&mf);
                break;
            }
            createBy=0;
            base=nullptr;
        }
    };
    class OutputStream{
        public:
        virtual const char *write(void *data,int length)=0;
    };
    class MemoryChunkOutputStream:public OutputStream{
        public:
        pxprpc_rtbridge_base::MemoryChunk *chunk=nullptr;
        int32_t pos=0;
        void target(pxprpc_rtbridge_base::MemoryChunk *chunkIn){
            chunk=chunkIn;
            pos=0;
        }
        //Data exceed the buffer size will be discard. '->pos' will still increase, To get the properly size. 
        virtual const char *write(void *data,int length){
            if(pos<chunk->size){
                memmove(((char *)chunk->base)+pos,data,std::min(chunk->size-pos,length));
            }
            pos+=length;
            return nullptr;
        }
        virtual ~MemoryChunkOutputStream(){
        }
    };
    bool inited=false;
    void init(){
        if(!inited){
            defaultFuncMap.add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe_pp.serve",
            [](auto para,auto ret)->void {
                auto obj=new PxpPipeServer(para->nextString());
                obj->serve();
                ret->resolve(obj);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe_pp.accept",
            [](auto para,auto ret)->void {
                auto serv=static_cast<PxpPipeServer *>(para->nextObject());
                serv->accept([ret](auto io)->void {
                    if(io==nullptr){
                        ret->reject("Accept aborted");
                    }else{
                        ret->resolve(new PxpConnection(io));
                    }
                });
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pipe_pp.connect",
            [](auto para,auto ret)->void {
                auto servName=para->nextString();
                ret->resolve(new PxpConnection(pxprpc_pipe_connect(servName.c_str())));
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_to_raw_addr",
            [](auto para,auto ret)->void {
                auto ioaddr=reinterpret_cast<int64_t>(static_cast<PxpConnection *>((para->nextObject()))->io);
                ret->resolve(ioaddr);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_from_raw_addr",
            [](auto para,auto ret)->void {
                ret->resolve(new PxpConnection(reinterpret_cast<struct pxprpc_abstract_io *>(para->nextLong())));
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_send",
            [](auto para,auto ret)->void {
                auto ioaddr=static_cast<PxpConnection *>((para->nextObject()))->io;
                auto buffer1=new pxprpc_buffer_part();
                auto p2=para->nextBytes();
                buffer1->bytes.length=std::get<0>(p2);
                buffer1->bytes.base=std::get<1>(p2);
                buffer1->next_part=nullptr;
                auto cb=new std::function<void()>([ret,buffer1,ioaddr]()->void {
                    auto err=ioaddr->send_error;
                    if(err!=nullptr){
                        ret->reject(err);
                    }else{
                        ret->resolve();
                    }
                    delete buffer1;
                });
                ioaddr->send(ioaddr,buffer1,pxprpc::callAndFreeCppFunction,cb);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_receive",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para,pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto ioaddr=static_cast<PxpConnection *>((para->nextObject()))->io;
                auto buffer1=new pxprpc_buffer_part();
                buffer1->next_part=NULL;
                auto cb=new std::function<void()>([ret,buffer1,ioaddr]()->void {
                    auto err=ioaddr->receive_error;
                    if(err!=nullptr){
                        ret->reject(err);
                    }else{
                        ret->resolve(*buffer1,[ioaddr,buffer1]()->void {
                            ioaddr->buf_free(buffer1->bytes.base);
                        });
                    }
                    delete buffer1;
                });
                ioaddr->receive(ioaddr,&buffer1->bytes,pxprpc::callAndFreeCppFunction,cb);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_pp.io_set_auto_close",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para,auto ret)->void {
                auto conn=static_cast<PxpConnection *>((para->nextObject()));
                auto autoClose=para->nextBool();
                //connection will not be closed when connection is deleted. Take care of connection leak.
                conn->setAutoClose(autoClose);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge_host.new_tcp_rpc_server",
            [](auto para,auto ret)->void {
                auto ser=para->asSerializer();
                auto host=ser->getString();
                auto port=ser->getInt();
                auto tcp=new pxprpc_rtbridge_host::TcpPxpRpcServer(host,port);
                ret->resolve(tcp);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge.memory_alloc",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto size=para->nextInt();
                auto chunk=new MemoryChunk();
                chunk->alloc(size);
                ret->resolve(chunk);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge.memory_access",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto addr=para->nextLong();
                auto size=para->nextInt();
                auto chunk=new MemoryChunk();
                chunk->access(reinterpret_cast<void *>(addr),size);
                ret->resolve(chunk);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge.memory_read",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto chunk=static_cast<MemoryChunk *>(para->nextObject());
                auto offset=para->nextInt();
                auto size=para->nextInt();
                if(size==-1){
                    size=chunk->size;
                }
                ret->resolve((char *)chunk->base+offset,size);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge.memory_write",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto chunk=static_cast<MemoryChunk *>(para->nextObject());
                auto offset=para->nextInt();
                auto data=para->nextBytes();
                memmove((char *)chunk->base+offset,std::get<1>(data),std::get<0>(data));
                ret->resolve();
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge.memory_info",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto chunk=static_cast<MemoryChunk *>(para->nextObject());
                auto ser=new pxprpc::Serializer();
                ser->prepareSerializing(16);
                ser->putLong(reinterpret_cast<int64_t>(chunk->base));
                ser->putInt(chunk->size);
                ret->resolve(ser);
            })).add((new pxprpc::NamedFunctionPPImpl1())->init("pxprpc_rtbridge.memory_mapfile",
            [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret)->void {
                auto path=para->nextString();
                auto mode=para->nextString();
                auto size=para->nextInt();
                auto fm=new MemoryChunk();
                auto err=fm->filemap(path,mode,size);
                if(err!=nullptr){
                    ret->reject(err);
                    delete fm;
                }else{
                    ret->resolve(fm);
                }
            }));
        }
    }
}
