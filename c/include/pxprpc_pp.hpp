

#pragma once

extern "C"{
#include <pxprpc.h>
}

#include <string>
#include <functional>
#include <tuple>
#include <vector>
#include <unordered_map>


namespace pxprpc{

void callAndFreeCppFunction(void *cppFn){
    auto fn=static_cast<std::function<void()> *>(cppFn);
    (*fn)();
    delete fn;
}

namespace iopp{
    void send(struct pxprpc_abstract_io *io1,std::vector<std::tuple<int32_t,void *>> bufs,std::function<void(const char *err)> cb){
        int size=bufs.size();
        auto bufList=new pxprpc_buffer_part[size];
        for(int i1=0;i1<size;i1++){
            bufList[i1].bytes.base=std::get<1>(bufs[i1]);
            bufList[i1].bytes.length=std::get<0>(bufs[i1]);
            if(i1+1<size){
                bufList[i1].next_part=&bufList[i1+1];
            }else{
                bufList[i1].next_part=nullptr;
            }
        }
        auto cCb=new std::function<void()>([io1,cb,bufList]()->void {
            auto err=io1->send_error;
            delete[] bufList;
            cb(err);
        });
        io1->send(io1,bufList,callAndFreeCppFunction,cCb);
    }
    //cb:(err,buffer,freeBuf)->void
    void receive(struct pxprpc_abstract_io *io1,
        std::function<void(const char *err,std::tuple<int32_t,void *>,std::function<void()>)> cb){
        auto recvBuf=new pxprpc_bytes();
        auto cCb=new std::function<void()>([io1,cb,recvBuf]()->void {
            auto err=io1->receive_error;
            auto base=recvBuf->base;
            auto len=recvBuf->length;
            cb(err,std::make_tuple(len,base),[io1,base]()->void {
                io1->buf_free(base);
            });
            delete recvBuf;
            
        });
        io1->receive(io1,recvBuf,callAndFreeCppFunction,cCb);
    }
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
    uint32_t getVarint(){
        uint32_t val;
        pxprpc_ser_get_varint(&this->wrapped,&val);
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
    Serializer *putVarint(uint32_t val){
        pxprpc_ser_put_varint(&this->wrapped,val);
        return this;
    }
    Serializer *putString(const std::string& val){
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
    static void freeBuiltBuffer(void *bufferBase){
        pxprpc_ser_free_buf(bufferBase);
    }
};

static void __wrap_call(pxprpc_callable *self,pxprpc_request *r);

pxprpc_server_api *servapi;

void init(){
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

void __wrap_requestOnFinish(pxprpc_request *r);

class PxpRequestWrap{
    public:
    pxprpc_request *req=NULL;
    void *callableDataPP=NULL;
    std::function<void(PxpRequestWrap *)> onFinishPP=[](PxpRequestWrap *)->void{};
    static PxpRequestWrap *wrap(pxprpc_request *req){
        if(req->callable_data!=NULL){
            return static_cast<PxpRequestWrap *>(req->callable_data);
        }else{
            auto r=new PxpRequestWrap();
            r->req=req;
            req->callable_data=r;
            req->on_finish=__wrap_requestOnFinish;
            return r;
        }
    }
    PxpServContextWrap context(){
        return PxpServContextWrap::wrap(req->server_context);
    }
    void nextStep(){
        req->next_step(req);
    }
    struct pxprpc_bytes *parameter(){
        return &req->parameter;
    }
    struct pxprpc_buffer_part &result(){
        return req->result;
    }
    void setRejected(bool rejected){
        req->rejected=rejected?1:0;
    }
    void onWrapFree(){
        this->onFinishPP(this);
        delete this;
    }
};

void __wrap_requestOnFinish(pxprpc_request *r){
    auto wrap=static_cast<PxpRequestWrap *>(r->callable_data);
    wrap->onWrapFree();
}

class NamedFunctionPP{
    public:
    pxprpc_callable callable;
    std::string name;

    NamedFunctionPP(){
        this->callable.user_data=this;
        this->callable.call=__wrap_call;
    }

    NamedFunctionPP *setName(std::string& funcName){
        this->name=funcName;
        return this;
    }
    
    virtual void call(PxpRequestWrap *r)=0;
    virtual ~NamedFunctionPP(){}
};



static pxprpc_callable *__wrap_funcmap_get(pxprpc_funcmap *self,char *funcname,int namelen);

class FunctionMap{
    public:
    std::unordered_map<std::string,pxprpc_callable *> map;
    pxprpc_funcmap cfm;
    FunctionMap& add(std::string name,pxprpc_callable *fn){
        this->map[name]=fn;
        return *this;
    }
    FunctionMap& add(NamedFunctionPP *func){
        this->map[func->name]=&func->callable;
        return *this;
    }
    pxprpc_funcmap *cFuncmap(){
        this->cfm.get=__wrap_funcmap_get;
        this->cfm.p=this;
        return &this->cfm;
    }
    pxprpc_callable *get(std::string name){
        auto found=this->map.find(name);
        if(found==this->map.end()){
            return nullptr;
        }else{
            return found->second;
        }
    }
};

FunctionMap defaultFuncMap;

static pxprpc_callable *__wrap_funcmap_get(pxprpc_funcmap *self,char *funcname,int namelen){
    FunctionMap *map=static_cast<FunctionMap *>(self->p);
    return map->get(std::string(funcname,funcname+namelen));
}


static void __wrap_call(pxprpc_callable *self,pxprpc_request *r){
    NamedFunctionPP *selfpp=static_cast<NamedFunctionPP *>(self->user_data);
    selfpp->call(PxpRequestWrap::wrap(r));
}


}
