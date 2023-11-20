

#pragma once


extern "C"{
#include <pxprpc.h>
}

#include <functional>
#include <tuple>
#include <vector>

namespace pxprpc{
    


static void __wrap_readParameter(struct pxprpc_callable *self,struct pxprpc_request *r,void (*doneCallback)(struct pxprpc_request *r));


static void __wrap_call(struct pxprpc_callable *self,struct pxprpc_request *r,void (*onResult)(struct pxprpc_request *r,struct pxprpc_object *result));


static void __wrap_writeResult(struct pxprpc_callable *self,struct pxprpc_request *r);

pxprpc_server_api *servapi;

extern void init(){
    pxprpc_server_query_interface(&servapi);
}

static uint32_t __wrap_object_addRef(struct pxprpc_object *self);
static uint32_t __wrap_object_release(struct pxprpc_object *self);

class RpcObject{
    struct pxprpc_object cObj;
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

class RpcRawBytes:public RpcObject{
    public:
    virtual ~RpcRawBytes(){
    }
    uint8_t *data;
    int size;
    RpcRawBytes(uint8_t *data,int size):RpcObject(){
        this->data=data;
        this->size=size;
    }
    virtual struct pxprpc_object *cObject(){
        auto r1=pxprpc::servapi->new_bytes_object(this->size);
        pxprpc::servapi->fill_bytes_object(r1,this->data,this->size);
        delete this;
        return r1;
    }
};


static uint32_t __wrap_object_addRef(struct pxprpc_object *self){
    auto rpcObj=static_cast<RpcObject *>(self->object1);
    rpcObj->addRef();
}

static uint32_t __wrap_object_release(struct pxprpc_object *self){
    auto rpcObj=static_cast<RpcObject *>(self->object1);
    rpcObj->release();
}

static void __NamedFunctionPPIoRead(void *p);

class NamedFunctionPP;
class __IoReadArg{
    public:
    std::function<void(struct pxprpc_abstract_io *,const char *)> doneCallback;
    struct pxprpc_abstract_io *io1;
    NamedFunctionPP *callable;
};


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
        std::function<void(struct pxprpc_abstract_io *,const char *)> doneCallback){
            auto arg1=new __IoReadArg();
            arg1->callable=this;
            arg1->doneCallback=doneCallback;
            arg1->io1=io1;
            io1->read(io1,length,buf,&__NamedFunctionPPIoRead,arg1);
    }
    NamedFunctionPP(std::string funcName){
        this->callable.userData=this;
        this->callable.readParameter=__wrap_readParameter;
        this->callable.call=__wrap_call;
        this->callable.writeResult=__wrap_writeResult;
        this->mCNamedFunc.callable=&this->callable;
        this->name=funcName;
        this->mCNamedFunc.name=this->name.c_str();
    }
    virtual struct pxprpc_namedfunc *cNamedFunc(){
        return &mCNamedFunc;
    }
    virtual ~NamedFunctionPP(){
    }
};


class Serializer{
    public:
    struct pxprpc_serializer wrapped;
    bool serializing=false;
    Serializer *prepareUnserializing(uint8_t *buf,uint32_t size){
        pxprpc_ser_prepare_unser(&this->wrapped,buf,size);
        this->serializing=false;
        return this;
    }
    Serializer *prepareSerializing(uint32_t initBufSize){
        pxprpc_ser_prepare_ser(&this->wrapped,initBufSize);
        this->serializing=true;
        return this;
    }
    int32_t getInt(){
        int32_t val;
        pxprpc_ser_get_int(&this->wrapped,&val,1);
        return val;
    }
    int64_t getLong(){
        int64_t val;
        pxprpc_ser_get_long(&this->wrapped,&val,1);
        return val;
    }
    float getFloat(){
        float val;
        pxprpc_ser_get_float(&this->wrapped,&val,1);
        return val;
    }
    double getDouble(){
        double val;
        pxprpc_ser_get_double(&this->wrapped,&val,1);
        return val;
    }
    std::vector<int32_t> getIntVec(int size){
        std::vector<int32_t> vec(size);
        pxprpc_ser_get_int(&this->wrapped,vec.data(),size);
        return vec;
    }
    std::vector<int64_t> getLongVec(int size){
        std::vector<int64_t> vec(size);
        pxprpc_ser_get_long(&this->wrapped,vec.data(),size);
        return vec;
    }
    std::vector<float> getFloatVec(int size){
        std::vector<float> vec(size);
        pxprpc_ser_get_float(&this->wrapped,vec.data(),size);
        return vec;
    }
    std::vector<double> getDoubleVec(int size){
        std::vector<double> vec(size);
        pxprpc_ser_get_double(&this->wrapped,vec.data(),size);
        return vec;
    }
    std::tuple<int,uint8_t*> getBytes(){
        uint32_t size;
        uint8_t *val=pxprpc_ser_get_bytes(&this->wrapped,&size);
        return std::tuple<int,uint8_t*>(size,val);
    }
    Serializer *putInt(int32_t val){
        pxprpc_ser_put_int(&this->wrapped,&val,1);
        return this;
    }
    Serializer *putLong(int64_t val){
       pxprpc_ser_put_long(&this->wrapped,&val,1);
        return this;
    }
    Serializer *putFloat(float val){
        pxprpc_ser_put_float(&this->wrapped,&val,1);
        return this;
    }
    Serializer *putDouble(double val){
        pxprpc_ser_put_double(&this->wrapped,&val,1);
        return this;
    }
    Serializer *putBytes(std::tuple<int,uint8_t*> val){
        pxprpc_ser_put_bytes(&this->wrapped,std::get<1>(val),std::get<0>(val));
        return this;
    }
};

static void __NamedFunctionPPIoRead(void *p){
    auto *arg0=static_cast<__IoReadArg *>(p);
    arg0->doneCallback(arg0->io1,
        arg0->io1->get_error(arg0->io1,(void *)arg0->io1->read));
    delete arg0;
}





static void __wrap_readParameter(struct pxprpc_callable *self,struct pxprpc_request *r,void (*doneCallback)(struct pxprpc_request *r)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->readParameter(r,[doneCallback,r]()->void{doneCallback(r);});
};


static void __wrap_call(struct pxprpc_callable *self,struct pxprpc_request *r,void (*onResult)(struct pxprpc_request *r,struct pxprpc_object *result)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->call(r,[onResult,r](pxprpc::RpcObject *resultObj)->void{
        if(resultObj==NULL)
            onResult(r,NULL);
        else
            onResult(r,resultObj->cObject());
    });
}

static void __wrap_writeResult(struct pxprpc_callable *self,struct pxprpc_request *r){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->writeResult(r);
}


}