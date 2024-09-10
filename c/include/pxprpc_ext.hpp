


#pragma once

#include "pxprpc_pp.hpp"

namespace pxprpc{

class TableSerializer{
    protected:
    Serializer *ser=nullptr;
    int32_t flag=0;
    int32_t rowCnt=0;
    std::string columnType;
    std::vector<std::string> columnName;
    std::vector<void *> cols;
    public:
    static const int FLAG_NO_COLUMN_NAME=1;
    TableSerializer *bindSerializer(Serializer *ser){
        this->ser=ser;
        return this;
    }
    TableSerializer *load(std::tuple<int,uint8_t *> data){
        this->ser=(new Serializer())->prepareUnserializing(std::get<1>(data),std::get<0>(data));
        this->load();
        return this;
    }
    TableSerializer *load(){
        this->flag=this->ser->getVarint();
        this->rowCnt=this->ser->getVarint();
        this->columnType=this->ser->getString();
        this->columnName.clear();
        if(!(this->flag&FLAG_NO_COLUMN_NAME)){
            for(int i1=0;i1<this->columnType.size();i1++){
                this->columnName.push_back(this->ser->getString());
            }
        }
        for(auto typ=this->columnType.begin();typ!=this->columnType.end();typ++){
            switch(*typ){
                case 'i':{
                    auto col=new std::vector<int32_t>();
                    auto col2=this->ser->getIntVec(this->rowCnt);
                    col->swap(col2);
                    this->cols.push_back(col);
                }
                break;
                case 'l':{
                    auto col=new std::vector<int64_t>();
                    auto col2=this->ser->getLongVec(this->rowCnt);
                    col->swap(col2);
                    this->cols.push_back(col);
                }
                break;
                case 'f':{
                    auto col=new std::vector<float>();
                    auto col2=this->ser->getFloatVec(this->rowCnt);
                    col->swap(col2);
                    this->cols.push_back(col);
                }
                break;
                case 'd':{
                    auto col=new std::vector<double>();
                    auto col2=this->ser->getDoubleVec(this->rowCnt);
                    col->swap(col2);
                    this->cols.push_back(col);
                }
                break;
                case 'b':{
                    auto col=new std::vector<std::tuple<int,uint8_t *>>(rowCnt);
                    for(int i1=0;i1<rowCnt;i1++){
                        (*col)[i1]=this->ser->getBytes();
                    }
                    this->cols.push_back(col);
                }
                break;
                case 's':{
                    auto col=new std::vector<std::string>(rowCnt);
                    for(int i1=0;i1<rowCnt;i1++){
                        (*col)[i1]=this->ser->getString();
                    }
                    this->cols.push_back(col);
                }
                break;
                case 'c':{
                    auto col=new std::vector<uint8_t>(rowCnt);
                    for(int i1=0;i1<rowCnt;i1++){
                        (*col)[i1]=this->ser->getVarint();
                    }
                    this->cols.push_back(col);
                }
                break;
            }
        }
        return this;
    }
    //Create new Serializer if bindSerializer not called, caller take response to delete the created Serializer.
    Serializer *buildSer(){
        if(this->ser==nullptr){
            this->ser=(new Serializer())->prepareSerializing(16);
        }
        int flag=0;
        if(this->columnName.size()==0 && this->columnType.size()>0){
            flag|=FLAG_NO_COLUMN_NAME;
        };
        this->ser->putVarint(flag);
        this->ser->putVarint(this->rowCnt);
        this->ser->putString(this->columnType);
        if(!(flag & FLAG_NO_COLUMN_NAME)){
            for(auto name=this->columnName.begin();name!=this->columnName.end();name++){
                this->ser->putString(*name);
            }
        }
        
        for(int i1=0;i1<this->columnType.length();i1++){
            switch(this->columnType.at(i1)){
                case 'i':{
                    auto col=reinterpret_cast<std::vector<int32_t>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putInt(*it);
                    }
                }
                break;
                case 'l':{
                    auto col=reinterpret_cast<std::vector<int64_t>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putLong(*it);
                    }
                }
                break;
                case 'f':{
                    auto col=reinterpret_cast<std::vector<float>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putFloat(*it);
                    }
                }
                break;
                case 'd':{
                    auto col=reinterpret_cast<std::vector<double>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putDouble(*it);
                    }
                }
                break;
                case 'b':{
                    auto col=reinterpret_cast<std::vector<std::tuple<int,uint8_t*>>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putBytes(std::get<1>(*it),std::get<0>(*it));
                    }
                }
                break;
                case 's':{
                    auto col=reinterpret_cast<std::vector<std::string>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putString(*it);
                    }
                }
                break;
                case 'c':{
                    auto col=reinterpret_cast<std::vector<uint8_t>*>(this->cols[i1]);
                    for(auto it=col->begin();it!=col->end();it++){
                        this->ser->putVarint(*it);
                    }
                }
                break;
            }
        }
        return this->ser;
    }
    
    int32_t *getInt32Column(int index){
        return reinterpret_cast<std::vector<int32_t> *>(this->cols[index])->data();
    }
    int64_t *getInt64Column(int index){
        return reinterpret_cast<std::vector<int64_t> *>(this->cols[index])->data();
    }
    float *getFloatColumn(int index){
        return reinterpret_cast<std::vector<float> *>(this->cols[index])->data();
    }
    double *getDoubleColumn(int index){
        return reinterpret_cast<std::vector<double> *>(this->cols[index])->data();
    }
    std::string *getStringColumn(int index){
        return reinterpret_cast<std::vector<std::string> *>(this->cols[index])->data();
    }
    std::tuple<int,uint8_t> *getBytesColumn(int index){
        return reinterpret_cast<std::vector<std::tuple<int,uint8_t>> *>(this->cols[index])->data();
    }
    uint8_t *getBoolColumn(int index){
        return reinterpret_cast<std::vector<uint8_t> *>(this->cols[index])->data();
    }
    TableSerializer *addRow(void **row){
        auto size=this->columnType.size();
        for(int i1=0;i1<size;i1++){
            switch(this->columnType.at(i1)){
                case 'i':
                reinterpret_cast<std::vector<int32_t> *>(this->cols[i1])->push_back(*reinterpret_cast<int32_t *>(row[i1]));
                break;
                case 'l':
                reinterpret_cast<std::vector<int64_t> *>(this->cols[i1])->push_back(*reinterpret_cast<int64_t *>(row[i1]));
                break;
                case 'f':
                reinterpret_cast<std::vector<float> *>(this->cols[i1])->push_back(*reinterpret_cast<float *>(row[i1]));
                break;
                case 'd':
                reinterpret_cast<std::vector<double> *>(this->cols[i1])->push_back(*reinterpret_cast<double *>(row[i1]));
                break;
                case 'b':
                reinterpret_cast<std::vector<std::tuple<int,uint8_t *>> *>(this->cols[i1])->push_back(*reinterpret_cast<std::tuple<int,uint8_t *> *>(row[i1]));
                break;
                case 's':
                reinterpret_cast<std::vector<std::string> *>(this->cols[i1])->push_back(*reinterpret_cast<std::string *>(row[i1]));
                break;
                case 'c':
                reinterpret_cast<std::vector<uint8_t> *>(this->cols[i1])->push_back(*reinterpret_cast<uint8_t *>(row[i1]));
                break;
            }
        }
        this->rowCnt+=1;
        return this;
    }
    TableSerializer *setColumnInfo(std::string columnType,std::vector<std::string> columnName){
        this->columnType=columnType;
        this->columnName=columnName;
        for(int i1=0;i1<columnType.size();i1++){
            switch(columnType.at(i1)){
                case 'i':
                this->cols.push_back(new std::vector<int32_t>());
                break;
                case 'l':
                this->cols.push_back(new std::vector<int64_t>());
                break;
                case 'f':
                this->cols.push_back(new std::vector<float>());
                break;
                case 'd':
                this->cols.push_back(new std::vector<double>());
                break;
                case 'b':
                this->cols.push_back(new std::vector<std::tuple<int,uint8_t *>>());
                break;
                case 's':
                this->cols.push_back(new std::vector<std::string>());
                break;
                case 'c':
                this->cols.push_back(new std::vector<uint8_t>());
                break;
            }
        }
        return this;
    }
    TableSerializer *setColumnInfo(std::string columnType){
        return this->setColumnInfo(columnType,std::vector<std::string>());
    }
    std::vector<std::string> getColumnsName(){
        return this->columnName;
    }
    std::string getColumnType(){
        return this->columnType;
    }
    int getRowCount(){
        return this->rowCnt;
    }
    ~TableSerializer(){
        for(int i1=0;i1<this->cols.size()&&i1<this->columnType.size();i1++){
            auto col=this->cols[i1];
            if(col!=nullptr){
                switch(this->columnType.at(i1)){
                    case 'i':
                    delete reinterpret_cast<std::vector<int32_t> *>(col);
                    break;
                    case 'l':
                    delete reinterpret_cast<std::vector<int64_t> *>(col);
                    break;
                    case 'f':
                    delete reinterpret_cast<std::vector<float> *>(col);
                    break;
                    case 'd':
                    delete reinterpret_cast<std::vector<double> *>(col);
                    break;
                    case 'b':
                    delete reinterpret_cast<std::vector<std::tuple<int,uint8_t *>> *>(col);
                    break;
                    case 's':
                    delete reinterpret_cast<std::vector<std::string> *>(col);
                    break;
                    case 'c':
                    delete reinterpret_cast<std::vector<uint8_t> *>(col);
                    break;
                }
                this->cols[i1]=nullptr;
            }
        }
        
    }
};


class NamedFunctionPPImpl1:public NamedFunctionPP{
    public:
    class AsyncReturn{
        public:
        PxpRequestWrap *req;
        std::function<void()> onReqFinished=[]()->void{};
        void resolve(struct pxprpc_buffer_part &buf,std::function<void()> onReqFinished){
            req->result()=buf;
            onReqFinished=this->onReqFinished;
            req->nextStep();
        }
        //"buf" will be copied
        void resolve(void *buf,size_t size){
            auto rawbuf=new uint8_t[size];
            req->result().bytes.base=rawbuf;
            req->result().bytes.length=size;
            req->result().next_part=nullptr;
            memmove(req->result().bytes.base,buf,size);
            this->onReqFinished=[rawbuf]()->void{
                delete rawbuf;
            };
            req->nextStep();
        }
        //"ser" will be deleted by callee.
        void resolve(Serializer *ser){
            ser->buildPxpBytes(&req->result().bytes);
            this->onReqFinished=[ser,this]()->void{
                ser->freeBuiltBuffer(req->result().bytes.base);
                delete ser;
            };
            req->nextStep();
        }
        void resolve(){
            req->result().bytes.length=0;
            req->result().bytes.base=nullptr;
            req->nextStep();
        }
        void resolve(int32_t v){
            auto ser=new Serializer();
            ser->prepareSerializing(8)->putInt(v);
            resolve(ser);
        }
        void resolve(int64_t v){
            auto ser=new Serializer();
            ser->prepareSerializing(8)->putLong(v);
            resolve(ser);
        }
        void resolve(float v){
            auto ser=new Serializer();
            ser->prepareSerializing(8)->putFloat(v);
            resolve(ser);
        }
        void resolve(double v){
            auto ser=new Serializer();
            ser->prepareSerializing(8)->putDouble(v);
            resolve(ser);
        }
        void resolve(std::string v){
            auto ser=new Serializer();
            ser->prepareSerializing(8)->putString(v);
            resolve(ser);
        }
        void resolve(PxpObject *obj){
            int32_t refid=this->req->context().allocRef(obj);
            resolve(refid);
        }
        void reject(const std::string &errorMessage){
            auto len=errorMessage.length();
            auto rawbuf=new uint8_t[len];
            req->result().bytes.base=rawbuf;
            req->result().bytes.length=len;
            req->result().next_part=nullptr;
            memmove(req->result().bytes.base,errorMessage.c_str(),len);
            req->onFinishPP=[rawbuf](PxpRequestWrap *req)->void{
                delete rawbuf;
            };
            req->setRejected(true);
            req->nextStep();
        }
        
    };
    class Parameter{
        public:
        PxpRequestWrap *req;
        Serializer *ser=nullptr; 
        Serializer *asSerializer(){
            if(this->ser==nullptr){
                this->ser=Serializer::fromPxpBytes(req->parameter());
            }
            return this->ser;
        }
        struct pxprpc_bytes *asRaw(){
            return req->parameter();
        }
        //nextXXX will use asSerializer
        int32_t nextInt(){
            return this->asSerializer()->getInt();
        }
        int64_t nextLong(){
            return this->asSerializer()->getLong();
        }
        float nextFloat(){
            return this->asSerializer()->getFloat();
        }
        double nextDouble(){
            return this->asSerializer()->getDouble();
        }
        PxpObject *nextObject(){
            return req->context().dereference(this->nextInt());
        }
        std::string nextString(){
            return this->asSerializer()->getString();
        }
        std::tuple<int,uint8_t *> nextBytes(){
            return this->asSerializer()->getBytes();
        }
        virtual ~Parameter(){
            if(this->ser!=nullptr){
                delete this->ser;
            }
        }
    };
    std::function<void(Parameter *para,AsyncReturn *ret)> handler=[](auto a,auto b)->void{};
    virtual NamedFunctionPPImpl1 *init(std::string name,std::function<void(Parameter *para,AsyncReturn *ret)> handler){
        this->handler=handler;
        this->setName(name);
        return this;
    }
    virtual void call(PxpRequestWrap *r){
        auto para=new Parameter();
        para->req=r;
        auto ret=new AsyncReturn();
        ret->req=r;
        r->onFinishPP=[para,ret](auto req)->void{
            ret->onReqFinished();
            delete para;
            delete ret;
        };
        this->handler(para,ret);
    }
    virtual ~NamedFunctionPPImpl1(){}
};

}