

#include <pxprpc_rtbridge_host.hpp>

#include <pxprpc_pp.hpp>

/* constant name for runtime bridge "/pxprpc/runtime_bridge/0", can use pxprpc_pipe_connect to connect */
const char *pxprpc_rtbridge_rtbmanager_name="/pxprpc/runtime_bridge/0";

// For convenience, all pxprpc c++ module are import by "include". That mean, there is only one C++ compile unit for all C++ code. 
#include "pxprpc_rtbridge_mod.hpp"

namespace pxprpc_rtbridge_host{

pxprpc_server_api *servapi;


void _rtbmgrconnclosed(void *context){
    servapi->context_delete(&context);
}
void _rtbmgrconnhandler(struct pxprpc_abstract_io *io,void *p){
    pxprpc_server_context context;
    servapi->context_new(&context,io);
    struct pxprpc_server_context_exports *ctxexp=servapi->context_exports(context);
    ctxexp->on_closed=_rtbmgrconnclosed;
    ctxexp->cb_data=context;
    ctxexp->funcmap=pxprpc::defaultFuncMap.cFuncmap();
    servapi->context_start(context);
}


int inited=0;

pxprpc_libuv_api *uvapi;
std::tuple<pxprpc_server_libuv,uv_tcp_t> testtcp;


TcpPxpRpcServer::TcpPxpRpcServer(std::string host,int port){
    uv_tcp_init(uvloop,&this->tcp);
    this->host=host;
    this->port=port;
    running=false;
}

const char *TcpPxpRpcServer::start(){
    struct sockaddr_in saddr;
    int r=uv_ip4_addr(host.c_str(),port,&saddr);
    if(r){
        return "uv_ip4_addr failed.";
    }
    r=uv_tcp_bind(&tcp,(const sockaddr *)&saddr,0);
    if(r){
        return "uv_tcp_bind failed.";
    }
    server=uvapi->new_server(uvloop,(uv_stream_t *)&tcp,pxprpc::defaultFuncMap.cFuncmap());
    uvapi->serve_start(server);
    running=true;
    return nullptr;
}

const char *TcpPxpRpcServer::stop(){
    uvapi->delete_server(server);
    uv_close(reinterpret_cast<uv_handle_t *>(&tcp),nullptr);
    running=false;
    return nullptr;
}

TcpPxpRpcServer::~TcpPxpRpcServer(){
    if(running){
        stop();
    }
}

char *ensureInited(){
    if(inited==0){
        inited=1;
        pxprpc_libuv_query_interface(&uvapi);
        pxprpc_server_query_interface(&servapi);
        pxprpc_pipe_serve(pxprpc_rtbridge_rtbmanager_name,_rtbmgrconnhandler,NULL);
        pxprpc::init();
        char *err=pxprpc_rtbridge_init_and_run(reinterpret_cast<void **>(&uvloop));
        if(err!=nullptr)return err;
        for(int i=0;i<10000;i++){
            if(pxprpc_rtbridge_mod_init[i]==nullptr){
                break;
            }
            pxprpc_rtbridge_mod_init[i]();
        }
        inited=2;
        #ifdef PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER
        //let it leak?
        auto testtcp=new TcpPxpRpcServer("127.0.0.1",2048);
        testtcp->start();
        #endif
        inited=3;
        return nullptr;
    }else{
        return const_cast<char *>("inited");
    }
}
}

extern "C"{
    char *pxprpc_rtbridge_host_ensureInited(){
        return pxprpc_rtbridge_host::ensureInited();
    }
}



#ifdef PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER_EXE
static int pxprpc_rtbridge_host_running=1;
int main(int argc,char *argv[]){
    pxprpc_rtbridge_host::ensureInited();
    struct pxprpc_rtbridge_state state;
    pxprpc_rtbridge_get_current_state(&state);
    while(state.run_state!=0){
        uv_sleep(1000);
        pxprpc_rtbridge_get_current_state(&state);
    }
}
#endif




