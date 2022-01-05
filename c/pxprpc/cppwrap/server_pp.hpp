

#pragma once


extern "C"{
#include "../server.h"
#include <stdint.h>
}

#include <functional>

namespace pxprpc{
    


static void __wrap_readParameter(struct pxprpc_callable *self,struct pxprpc_request *r,void (*doneCallback)(struct pxprpc_request *r));


static void __wrap_call(struct pxprpc_callable *self,struct pxprpc_request *r,void (*onResult)(struct pxprpc_request *r,struct pxprpc_object *result));


static void __wrap_writeResult(struct pxprpc_callable *self,struct pxprpc_request *r);

pxprpc_server_api *servapi;

extern void init(){
    pxprpc_server_query_interface(&servapi);
}

static uint32_t __wrap_object_addRef(struct pxprpc_object *self){
    auto rpcObj=static_cast<RpcObject *>(self->object1);
    rpcObj->addRef();
}

static uint32_t __wrap_object_release(struct pxprpc_object *self){
    auto rpcObj=static_cast<RpcObject *>(self->object1);
    rpcObj->release();
}

class RpcObject{
    struct pxprpc_object cObj;
    uint32_t (*cObjOldRelease)(struct pxprpc_object *self);
    public:
    virtual ~RpcObject(){
    }
    virtual int addRef(){
        this->cObj.count++;
        return this->cObj.count;
    }
    virtual int release(){
        this->cObj.count--;
        if(this->cObj.count<=0){
            delete this;
            return 0;
        }else{
            return this->cObj.count;
        }
    }
    RpcObject(){
        this->cObj.object1=this;
        this->cObj.count=0;
        this->cObj.addRef=&__wrap_object_addRef;
        this->cObj.release=&__wrap_object_release;
    }
    virtual struct pxprpc_object *cObject(){
        return &cObj;
    }
};
class __IoReadArg{
    public:
    std::function<void(struct pxprpc_abstract_io *)> doneCallback;
    struct pxprpc_abstract_io *io1;
    NamedFunctionPP *callable;
};

static void __NamedFunctionPPIoRead(void *p);
class NamedFunctionPP{
    
    protected:
    struct pxprpc_callable callable;
    struct pxprpc_namedfunc mCNamedFunc;
    std::string name;
    public:
    virtual void readParameter(struct pxprpc_request *r,std::function<void()> doneCallback)=0;
    virtual void call(struct pxprpc_request *r,std::function<void(RpcObject *)> onResult)=0;
    virtual void writeResult(struct pxprpc_request *r)=0;
    void readFromIo(struct pxprpc_abstract_io *io1,uint8_t *buf,int length,
        std::function<void(struct pxprpc_abstract_io *)> doneCallback){
            auto arg1=new __IoReadArg();
            arg1->callable=this;
            arg1->doneCallback=doneCallback;
            arg1->io1=io1;
            io1->read(io1,length,buf,&__NamedFunctionPPIoRead,arg1);
    }
    NamedFunctionPP(std::string name){
        this->callable.userData=this;
        this->mCNamedFunc.callable=&this->callable;
        this->name=name;
        this->mCNamedFunc.name=name.c_str();
    }
    virtual struct pxprpc_namedfunc *cNamedFunc(){
        return &mCNamedFunc;
    }
    virtual ~NamedFunctionPP(){
    }
};

static void __NamedFunctionPPIoRead(void *p){
    auto *arg0=static_cast<__IoReadArg *>(p);
    arg0->doneCallback(arg0->io1);
    delete arg0;
}





void __wrap_readParameter(struct pxprpc_callable *self,struct pxprpc_request *r,void (*doneCallback)(struct pxprpc_request *r)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->readParameter(r,[doneCallback,r]()->void{doneCallback(r);});
};


void __wrap_call(struct pxprpc_callable *self,struct pxprpc_request *r,void (*onResult)(struct pxprpc_request *r,struct pxprpc_object *result)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
}

void __wrap_writeResult(struct pxprpc_callable *self,struct pxprpc_request *r);
}
