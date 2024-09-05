
#include <pxprpc_pp.hpp>
#include <queue>
extern "C"{
    #include <pxprpc.h>
    #include <pxprpc_pipe.h>
    #include <stdint.h>
}

namespace pxprpc_rtbirdgepp{        
    void __wrap_on_connect(struct pxprpc_abstract_io *io1,void *server);
    std::queue<struct pxprpc_abstract_io *> connected;
    using namespace pxprpc;
    class PxpPipeServer : public PxpObject{
        public:
        std::string s;
        std::function<void(struct pxprpc_abstract_io *)> newConn;
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
            pxprpc_pipe_serve(s.c_str(),__wrap_on_connect,this);
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
        }
    };
    void __wrap_on_connect(struct pxprpc_abstract_io *io1,void *server){
        auto serv=reinterpret_cast<PxpPipeServer *>(server);
        serv->onConnect(io1);
    }
    FunctionPPWithSerializer fn1("pxprpc_pipe.serve",[](PxpRequestWrap *r,Serializer *p,auto done)->void{
        auto res=(new Serializer())->prepareSerializing(16);
        auto obj=new PxpPipeServer(p->getString());
        auto refid=r->context().allocRef(obj);
        res->putInt(refid);
        done(res);
    });
    FunctionPPWithSerializer fn2("pxprpc_pipe.accept",[](PxpRequestWrap *r,Serializer *p,auto done)->void{
        auto servref=p->getInt();
        auto serv=static_cast<PxpPipeServer *>(r->context().dereference(servref));
        serv->accept([done](auto io)->void{
            auto ioi64=reinterpret_cast<int64_t>(io);
            done((new Serializer())->prepareSerializing(16)->putLong(ioi64));
        });
    });
    bool inited=false;
    void init(){
        if(!inited){
            defaultFuncMap.add(&fn1).add(&fn2);
        }
    }
}

extern "C"{
    extern pxprpc_funcmap *pxprpc_rtbridgepp_getfuncmap(){
        pxprpc_rtbirdgepp::init();
        return pxprpc::defaultFuncMap.cFuncmap();
    }
}