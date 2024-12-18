
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
    }

    //Post function to be run in host event loop/thread.(by use pxprpc_pipe_executor).
    //This function (and also "pxprpc_pipe_executor") can be call in other thread. 
    //Caution: Except these functions explicitly noted, pxprpc function should only called in "host thread".
    void postRunnable(std::function<void()> runnable){
        auto pFn=new std::function<void()>();
        *pFn=runnable;
        pxprpc_pipe_executor(__runCppFunction,pFn);
    }

}