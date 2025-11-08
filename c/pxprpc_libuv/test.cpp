

#include <iostream>

extern "C"{
    #include <pxprpc_libuv.h>
}


#include <pxprpc_pp.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>


uv_tcp_t servTcp;
uv_loop_t *loop;

uint8_t testFnWriteBuf[4];
const int constZero=0;
static void testNopCallback(void *p){};

using namespace pxprpc;


#include <pxprpc_ext.hpp>

int testPollCount=0;

using Parameter=NamedFunctionPPImpl1::Parameter;
using AsyncReturn=NamedFunctionPPImpl1::AsyncReturn;

void __TickEventDispatcherTimerCb(uv_timer_t *handle);
class TickEventDispatcher:public EventDispatcher{
    public:
    uv_timer_t timer;
    TickEventDispatcher(){
        uv_timer_init(uv_default_loop(),&timer);
        timer.data=static_cast<void *>(this);
        uv_timer_start(&timer,__TickEventDispatcherTimerCb,0,1000);
    }
    void tick(){
        if(pendingReturn!=nullptr){
            pendingReturn->resolve("tick");
        }
    }
    ~TickEventDispatcher(){
        uv_timer_stop(&timer);
    }
};
void __TickEventDispatcherTimerCb(uv_timer_t *handle){
    auto dispatcher=static_cast<TickEventDispatcher *>(handle->data);
    dispatcher->tick();
}

void defFunc(){
    defaultFuncMap.add((new NamedFunctionPPImpl1())->init("printString",
    [](Parameter* para,AsyncReturn* ret)->void {
        std::cout<<"enter function printString"<<std::endl;
        std::cout<<para->nextString()<<std::endl;
        ret->resolve("server:hello client");
        std::cout<<"exit function printString"<<std::endl;
    })).add((new NamedFunctionPPImpl1())->init("printSerilizedArgs",
    [](Parameter *para,AsyncReturn *ret)->void {
        std::cout<<"enter function printSerilizedArgs"<<std::endl;
        auto i=para->nextInt();
        auto l=para->nextLong();
        auto f=para->nextFloat();
        auto d=para->nextDouble();
        auto s=para->nextString();
        auto b=para->nextString();
        std::cout<<i<<","<<l<<","<<f<<","<<d<<","<<s<<","<<b<<std::endl;
        ret->resolve();
        std::cout<<"exit function printSerilizedArgs"<<std::endl;
    })).add((new NamedFunctionPPImpl1())->init("printSerilizedTable",
    [](Parameter *para,AsyncReturn* ret)->void {
        std::cout<<"enter function printSerilizedTable"<<std::endl;
        TableSerializer *tabser=new TableSerializer();
        tabser->bindSerializer(para->asSerializer())->load();
        auto colName=tabser->getColumnsName();
        std::cout<<"id\tname\tisdir\tfilesize\t"<<std::endl;
        for(int i1=0;i1<tabser->getRowCount();i1++){
            auto row=tabser->getRow(i1);
            std::cout<<row[0].i32<<"\t"<<row[1].str<<"\t"<<row[2].bool2<<"\t"<<row[3].i64<<std::endl;
        }
        delete tabser;
        
        tabser=new TableSerializer();
        tabser->setColumnsInfo("slc",std::vector<std::string>({"name","filesize","isdir"}));
        tabser->addRow({{"myfile.txt"},{123},{"myfile.txt"}});
        tabser->addValue("mydir")->addValue(0)->addValue(1);
        ret->resolve(tabser->buildSer());
        delete tabser;
        std::cout<<"exit function printSerilizedTable"<<std::endl;
    })).add((new NamedFunctionPPImpl1())->init("testDummyError",
    [](Parameter* para,AsyncReturn* ret)->void {
        std::cout<<"enter function testDummyError"<<std::endl;
        ret->reject("dummy error");
        std::cout<<"exit function testDummyError"<<std::endl;
    })).add((new NamedFunctionPPImpl1())->init("testPoll",
    [](Parameter* para,AsyncReturn* ret)->void {
        std::cout<<"enter function testPoll"<<std::endl;
        if(testPollCount<4){
            testPollCount++;
            ret->resolve(testPollCount);
        }else{
            ret->reject("poll stop error");
        }
        std::cout<<"exit function testPoll"<<std::endl;
    }));
    
    defaultFuncMap.add((new NamedFunctionPPImpl1())->init("testEvent",
    [](Parameter* para,AsyncReturn* ret)->void {
        std::cout<<"enter function testEvent"<<std::endl;
        ret->resolve(new TickEventDispatcher());
        std::cout<<"exit function testEvent"<<std::endl;
    }));

}


int main(int argc,char *argv[]){
    pxprpc::init();
    loop=uv_default_loop();
    
    memset(&servTcp,0,sizeof(uv_tcp_t));
    uv_tcp_init(loop,&servTcp);
    struct sockaddr_in saddr;
    int r=uv_ip4_addr("127.0.0.1",1089,&saddr);
    if(r){
        printf("uv_ip4_addr failed.");
    }
    r=uv_tcp_bind(&servTcp,(const sockaddr *)&saddr,0);
    if(r){
        printf("uv_tcp_bind failed.");
    }
    defFunc();
    auto rpc=pxprpc_libuv_server_new(loop,(uv_stream_t *)&servTcp,defaultFuncMap.cFuncmap());
    pxprpc_libuv_server_start(rpc);
    uv_run(loop,UV_RUN_DEFAULT);
    printf("libuv finish.");
    return 0;
}