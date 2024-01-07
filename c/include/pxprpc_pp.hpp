

#pragma once


extern "C"{
#include <pxprpc.h>
}

#include <string>
#include <functional>
#include <tuple>
#include <vector>

namespace pxprpc{
    

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
    std::string getString(){
        auto strbuf=getBytes();
        std::string str(reinterpret_cast<char *>(std::get<1>(strbuf)),std::get<0>(strbuf));
        return str;
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
    Serializer *putBytes(uint8_t *buf,uint32_t length){
        pxprpc_ser_put_bytes(&this->wrapped,buf,length);
        return this;
    }
    Serializer *putString(std::string val){
        putBytes((uint8_t *)val.c_str(),val.size());
        return this;
    }
    std::tuple<int,uint8_t*> build(){
        uint32_t size;
        auto *buf=pxprpc_ser_build(&this->wrapped,&size);
        return std::tuple<int,uint8_t*>((int)size,buf);
    }
    static Serializer *fromPxpBytes(struct pxprpc_bytes *bytes){
        auto ser=new Serializer();
        ser->prepareUnserializing(static_cast<uint8_t *>(bytes->base),bytes->length);
        return ser;
    }
    void buildPxpBytes(struct pxprpc_bytes *bytes){
        auto bb=build();
        bytes->base=std::get<1>(bb);
        bytes->length=std::get<0>(bb);
    }
};

static void __wrap_call(pxprpc_callable *self,pxprpc_request *r);

pxprpc_server_api *servapi;

extern void init(){
    pxprpc_server_query_interface(&servapi);
}

class PxpServContext;

class PxpObject{
    public:
    virtual ~PxpObject(){}
};


static void __wrap_ref_on_free(pxprpc_ref *ref){
    PxpObject *obj=static_cast<PxpObject *>(ref->object);
    delete obj;
}

class PxpServContextWrap{
    public:
    pxprpc_server_context ctx;
    static PxpServContextWrap wrap(pxprpc_server_context ctx){
        PxpServContextWrap r;
        r.ctx=ctx;
        return r;
    }
    uint32_t allocRef(PxpObject *obj){
        uint32_t index;
        auto cref=servapi->alloc_ref(this->ctx,&index);
        cref->object=obj;
        cref->on_free=__wrap_ref_on_free;
        return index;
    }
    void freeRef(uint32_t index){
        servapi->free_ref(this->ctx,&servapi->context_exports(ctx)->ref_pool[index]);
    }
    PxpObject *dereference(uint32_t index){
        return static_cast<PxpObject *>(servapi->context_exports(ctx)->ref_pool[index].object);
    }
};

class PxpRequestWrap;

void __wrap_requestOnFree(pxprpc_request *r);

class PxpRequestWrap{
    public:
    pxprpc_request *req=NULL;
    void *callableDataPP=NULL;
    std::function<void(PxpRequestWrap *)> onFreePP=[](PxpRequestWrap *)->void{};
    static PxpRequestWrap *wrap(pxprpc_request *req){
        if(req->callable_data!=NULL){
            return static_cast<PxpRequestWrap *>(req->callable_data);
        }else{
            auto r=new PxpRequestWrap();
            r->req=req;
            req->callable_data=r;
            return r;
        }
    }
    PxpServContextWrap context(){
        PxpServContextWrap::wrap(req->server_context);
    }
    void nextStep(){
        req->next_step(req);
    }
    struct pxprpc_bytes *parameter(){
        return &req->parameter;
    }
    void onWrapFree(){
        this->onFreePP(this);
        delete this;
    }
};

void __wrap_requestOnFree(pxprpc_request *r){
    auto wrap=static_cast<PxpRequestWrap *>(r->callable_data);
    wrap->onWrapFree();
}

class NamedFunctionPP{
    struct pxprpc_namedfunc mCNamedFunc;
    public:
    pxprpc_callable callable;
    std::string name;

    NamedFunctionPP(){
        this->callable.user_data=this;
        this->callable.call=__wrap_call;
        this->mCNamedFunc.callable=&this->callable;
    }

    NamedFunctionPP *setName(std::string funcName){
        this->name=funcName;
        this->mCNamedFunc.name=this->name.c_str();
        return this;
    }
    
    virtual void call(PxpRequestWrap *r)=0;
    virtual struct pxprpc_namedfunc *cNamedFunc(){
        return &mCNamedFunc;
    }
    virtual ~NamedFunctionPP(){}
};


class FunctionPPWithSerializer:public NamedFunctionPP{
    public:
    virtual void callWithSer(PxpRequestWrap *r,Serializer *parameter,std::function<void(Serializer *result)> done)=0;
    virtual void call(PxpRequestWrap *r){
        auto ser=Serializer::fromPxpBytes(r->parameter());
        r->onFreePP=[](PxpRequestWrap *r)->void{
            /* use callableDataPP? */
            pxprpc_ser_free_buf(r->req->result.bytes.base);
        };
        this->callWithSer(r,ser,[ser,r](Serializer *result)->void{
            delete ser;
            if(result!=nullptr){
                result->buildPxpBytes(&r->req->result.bytes);
                delete result;
                r->req->result.next_part=NULL;
            }else{
                r->req->result.bytes.length=0;
            }
            r->nextStep();
        });
    }
};

static void __wrap_call(pxprpc_callable *self,pxprpc_request *r){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->user_data);
    selfpp->call(PxpRequestWrap::wrap(r));
}

}