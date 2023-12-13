

#pragma once


extern "C"{
#include <pxprpc.h>
}

#include <functional>
#include <tuple>
#include <vector>

namespace pxprpc{
    


static void __wrap_readParameter(pxprpc_callable *self,pxprpc_request *r,void (*doneCallback)(pxprpc_request *r));


static void __wrap_call(pxprpc_callable *self,pxprpc_request *r,void (*onResult)(pxprpc_request *r,pxprpc_object *result));


static void __wrap_writeResult(pxprpc_callable *self,pxprpc_request *r,void (*doneCallback)(pxprpc_request *r));

pxprpc_server_api *servapi;

extern void init(){
    pxprpc_server_query_interface(&servapi);
}

static uint32_t __wrap_object_addRef(pxprpc_object *self);
static uint32_t __wrap_object_release(pxprpc_object *self);

class RpcObject{
    pxprpc_object cObj;
    public:
    static RpcObject *at(pxprpc_server_context context,int index){
        return (RpcObject *)servapi->context_exports(context)->ref_slots[index]->object1;
    }
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
    virtual pxprpc_object *cObject(){
        return &cObj;
    }
};

class RpcRawBytes:public RpcObject{
    public:
    static RpcRawBytes *at(pxprpc_server_context context,int index){
        pxprpc_bytes *obj=(pxprpc_bytes *)servapi->context_exports(context)->ref_slots[index]->object1;
        auto rrb=new RpcRawBytes((uint8_t *)&obj->data,obj->length);
        return rrb;
    }
    virtual ~RpcRawBytes(){
    }
    uint8_t *data;
    int size;
    RpcRawBytes(uint8_t *data,int size):RpcObject(){
        this->data=data;
        this->size=size;
    }
    virtual pxprpc_object *cObject(){
        auto r1=pxprpc::servapi->new_bytes_object(this->size);
        pxprpc::servapi->fill_bytes_object(r1,this->data,this->size);
        delete this;
        return r1;
    }
    std::string asString(){
        return std::string(reinterpret_cast<char *>(this->data),size);
    }
};



static uint32_t __wrap_object_addRef(pxprpc_object *self){
    auto rpcObj=static_cast<RpcObject *>(self->object1);
    rpcObj->addRef();
}

static uint32_t __wrap_object_release(pxprpc_object *self){
    auto rpcObj=static_cast<RpcObject *>(self->object1);
    rpcObj->release();
}



class NamedFunctionPP{
    
    protected:
    pxprpc_callable callable;
    struct pxprpc_namedfunc mCNamedFunc;
    std::string name;
    public:
    virtual void readParameter(pxprpc_request *r,std::function<void()> doneCallback)=0;
    virtual void call(pxprpc_request *r,std::function<void(RpcObject *)> onResult)=0;
    virtual void writeResult(pxprpc_request *r,std::function<void()> doneCallback)=0;
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

class IoPP;

static void __wrap_io_readCompleted(IoPP *p);
static void __wrap_io_writeCompleted(IoPP *p);

class IoPP{
    public:
    pxprpc_abstract_io *wrapped;
    IoPP(pxprpc_abstract_io *io){
        this->wrapped=io;
    }
    std::function<void(IoPP *,const char *)> onReadDone;
    void read(uint32_t length,uint8_t *buf,std::function<void(IoPP *,const char *)> onCompleted){
        this->onReadDone=onCompleted;
        this->wrapped->read(this->wrapped,length,buf,(void (*)(void *))__wrap_io_readCompleted,this);
    }
    std::function<void(IoPP *,const char *)> onWriteDone;
    void write(uint32_t length,const uint8_t *buf,std::function<void(IoPP *,const char *)> onCompleted){
        this->onWriteDone=onCompleted;
        this->wrapped->write(this->wrapped,length,buf,(void (*)(void *))__wrap_io_writeCompleted,this);
    }
};

static void __wrap_io_readCompleted(IoPP *p){
    p->onReadDone(p,p->wrapped->get_error(p->wrapped,(void *)p->wrapped->read));
}
static void __wrap_io_writeCompleted(IoPP *p){
    p->onWriteDone(p,p->wrapped->get_error(p->wrapped,(void *)p->wrapped->write));
}

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






static void __wrap_readParameter(pxprpc_callable *self,pxprpc_request *r,void (*doneCallback)(pxprpc_request *r)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->readParameter(r,[doneCallback,r]()->void{doneCallback(r);});
};


static void __wrap_call(pxprpc_callable *self,pxprpc_request *r,void (*onResult)(pxprpc_request *r,pxprpc_object *result)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->call(r,[onResult,r](pxprpc::RpcObject *resultObj)->void{
        if(resultObj==NULL)
            onResult(r,NULL);
        else
            onResult(r,resultObj->cObject());
    });
}

static void __wrap_writeResult(pxprpc_callable *self,pxprpc_request *r,void (*doneCallback)(pxprpc_request *r)){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->userData);
    selfpp->writeResult(r,[doneCallback,r]()->void{doneCallback(r);});
}


}