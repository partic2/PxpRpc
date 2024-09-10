
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

}