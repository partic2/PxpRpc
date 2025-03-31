
#pragma once

extern "C"{
    #include <pxprpc.h>
    #include <pxprpc_pipe.h>
    #include <uv.h>
    #include <pxprpc_libuv.h>
    #include <pxprpc_rtbridge.h>
}
#include <pxprpc_pp.hpp>
#include <pxprpc_ext.hpp>

namespace pxprpc_rtbridge_host{
    char *ensureInited();
    uv_loop_t *uvloop;
    
    class TcpPxpRpcServer : public pxprpc::PxpObject{
        public:
        pxprpc_server_libuv server;
        uv_tcp_t tcp;
        std::string host;
        int port;
        bool running;
        TcpPxpRpcServer(std::string host,int port);
        const char *start();
        const char *stop();
        ~TcpPxpRpcServer();
    };

    void __runCppFunction(void *cppFunc){
        auto fn=static_cast<std::function<void()> *>(cppFunc);
        (*fn)();
        delete fn;
    }

    //Post function to be run in host event loop/thread.(by use pxprpc_pipe_executor).
    //This function (and also "pxprpc_pipe_executor") can be call in other thread. 
    //Caution: Except these functions explicitly noted, pxprpc function should only called in "host thread".
    void postRunnable(std::function<void()> runnable){
        auto pFn=new std::function<void()>();
        *pFn=runnable;
        pxprpc_pipe_executor(__runCppFunction,pFn);
    }
    
    void __runCppFunctionWorkCb(uv_work_t *req){
        auto fn=static_cast<std::function<void()> *>(req->data);
        (*fn)();
        delete fn;
    }
    void __afterRunCppFunctionWorkCb(uv_work_t* req, int status){
        delete req;
    }
    

    //Thread safe resolve.
    template<typename ...Args>
    void resolveTS(pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret,Args... result){
        uv_sem_t sem;
        uv_sem_init(&sem,0);
        postRunnable([&sem,ret,result...]()-> void {
            ret->resolve(result...);
            uv_sem_post(&sem);
        });
        uv_sem_wait(&sem);
        uv_sem_destroy(&sem);
    }
    //Thread safe reject
    void rejectTS(pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret,const std::string& reason){
        uv_sem_t sem;
        uv_sem_init(&sem,0);
        postRunnable([&sem,ret,reason]()-> void {
            ret->reject(reason);
            uv_sem_post(&sem);
        });
        uv_sem_wait(&sem);
        uv_sem_destroy(&sem);
    }

    //Run function in libuv thread pool.
    void threadPoolRun(std::function<void()> runnable){
        auto pFn=new std::function<void()>();
        *pFn=runnable;
        auto work=new uv_work_t();
        work->data=(void *)pFn;
        uv_queue_work(uvloop,work,__runCppFunctionWorkCb,__afterRunCppFunctionWorkCb);
    }
}