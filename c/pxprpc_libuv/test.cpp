

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



pxprpc_libuv_api *pxpuv;
uv_tcp_t servTcp;
uv_loop_t *loop;

uint8_t testFnWriteBuf[4];
const int constZero=0;
static void testNopCallback(void *p){};

using namespace pxprpc;


FunctionPPWithSerializer fnPrintString("printString",[](auto r,auto parameter,auto done)->void{
    std::cout<<parameter->getString()<<std::endl;
    done((new Serializer())->prepareSerializing(8)->putString("server:hello client"));
});

FunctionPPWithSerializer fnPrintSerilizedArgs("printSerilizedArgs",[](auto r,auto parameter,auto done)->void{
    auto i=parameter->getInt();
    auto l=parameter->getLong();
    auto f=parameter->getFloat();
    auto d=parameter->getDouble();
    auto s=parameter->getString();
    auto b=parameter->getString();
    std::cout<<i<<","<<l<<","<<f<<","<<d<<","<<s<<","<<b<<std::endl;
    done(nullptr);
});



template<typename E>
int vectorIndexOf(std::vector<E> vec,E elem){
    auto found=std::find(vec.begin(),vec.end(),elem);
    if(found==vec.end())return -1;
    return std::distance(vec.begin(),found);
}

#include <pxprpc_ext.hpp>
FunctionPPWithSerializer fnPrintSerilizedTable("printSerilizedTable",[](auto r,auto parameter,auto done)->void{
    TableSerializer *tabser=new TableSerializer();
        tabser->bindSerializer(parameter)->load();
        auto colName=tabser->getColumnsName();
        auto nameCol=tabser->getStringColumn(vectorIndexOf(colName,std::string("name")));
        auto sizeCol=tabser->getInt64Column(vectorIndexOf(colName,std::string("filesize")));
        auto isDirCol=tabser->getBoolColumn(vectorIndexOf(colName,std::string("isdir")));
        std::cout<<"name\tfilesize\tisdir\t"<<std::endl;
        for(int i1=0;i1<tabser->getRowCount();i1++){
            std::cout<<colName[i1]<<"\t"<<sizeCol[i1]<<"\t"<<(isDirCol[i1]!=0)<<std::endl;
        }
        delete tabser;
        
        
        tabser=new TableSerializer();
        tabser->setHeader("slc",std::vector<std::string>({"name","filesize","isdir"}));
        std::string names[2]={"myfile.txt","mydir"};
        int64_t sizes[2]={123,0};
        uint8_t isdirs[2]={0,1};
        {
            void *row[3]={&names[0],&sizes[0],&isdirs[0]};
            tabser->addRow(row);
        }{
            void *row[3]={&names[0],&sizes[1],&isdirs[1]};
            tabser->addRow(row);
        }
        done(tabser->buildSer());
        delete tabser;
});


int main(int argc,char *argv[]){
    pxprpc::init();
    pxprpc_libuv_query_interface(&pxpuv);
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
    defaultFuncMap.add(&fnPrintString).add(&fnPrintSerilizedArgs).add(&fnPrintSerilizedTable);
    auto rpc=pxpuv->new_server(loop,(uv_stream_t *)&servTcp,defaultFuncMap.cFuncmap());
    pxpuv->serve_start(rpc);
    uv_run(loop,UV_RUN_DEFAULT);
    printf("libuv finish.");
    return 0;
}