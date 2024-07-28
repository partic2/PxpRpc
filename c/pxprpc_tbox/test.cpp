

#include <iostream>

#include <tbox/tbox.h>
extern "C"{
    #include <pxprpc_tbox.h>
}


#include <pxprpc_pp.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <string>

using namespace pxprpc;

pxprpc_tbox_api *srvtbox;



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
    pxprpc_tbox_query_interface(&srvtbox);
    if(tb_init(tb_null,tb_null)==tb_false){
        std::cerr<<"tb_init failed"<<errno<<std::endl;
    };
    tb_socket_ref_t sock=tb_socket_init(TB_SOCKET_TYPE_TCP,TB_IPADDR_FAMILY_IPV4);
    tb_ipaddr_t ipaddr;
    tb_ipaddr_clear(&ipaddr);
    if(tb_ipaddr_set(&ipaddr,"127.0.0.1",1089,TB_IPADDR_FAMILY_IPV4)==tb_false){
        std::cerr<<"tb_ipaddr_set failed"<<std::endl;
    }
    if(tb_socket_bind(sock,&ipaddr)==tb_false){
        std::cerr<<"tb_socket_bind failed"<<std::endl;
    }
    defaultFuncMap.add(&fnPrintString)->add(&fnPrintSerilizedArgs)->add(&fnPrintSerilizedTable);
    auto tbrpc=srvtbox->new_server(sock,defaultFuncMap.cFuncmap());
    srvtbox->serve_block(tbrpc);
    std::cerr<<"serve_block failed"<<srvtbox->get_error()<<std::endl;
    return 0;
}