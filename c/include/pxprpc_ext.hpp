


#pragma once

#include "pxprpc.h"
#include "pxprpc_pp.hpp"

namespace pxprpc{

class TableCellValue{
    public:
    union{
        bool bool2;
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        pxprpc_bytes bytes;
    };
    std::string str;
    TableCellValue(){}
    TableCellValue(int32_t v){i32=v;}
    TableCellValue(int64_t v){i64=v;}
    TableCellValue(float v){f32=v;}
    TableCellValue(double v){f64=v;}
    TableCellValue(std::string v){str=v;}
    TableCellValue(void* base,int32_t length){bytes.base=base;bytes.length=length;}
};

class TableSerializer{
    protected:
    Serializer *ser=nullptr;
    int32_t flag=0;
    int32_t rowCnt=0;
    std::string columnsType;
    std::vector<std::string> columnsName;
    std::vector<std::vector<TableCellValue>> cols;
    public:
    static const int FLAG_NO_COLUMN_NAME=1;
    TableSerializer *bindSerializer(Serializer *ser){
        this->ser=ser;
        return this;
    }
    TableSerializer *load(pxprpc_bytes *data){
        this->ser=(new Serializer())->prepareUnserializing((uint8_t *)data->base, data->length);
        this->load();
        delete this->ser;
        this->ser=nullptr;
        return this;
    }
    TableSerializer *load(std::tuple<int,uint8_t *> data){
        this->ser=(new Serializer())->prepareUnserializing(std::get<1>(data),std::get<0>(data));
        this->load();
        delete this->ser;
        this->ser=nullptr;
        return this;
    }
    TableSerializer *load(){
        this->flag=this->ser->getVarint();
        this->rowCnt=this->ser->getVarint();
        this->columnsType=this->ser->getString();
        this->columnsName.clear();
        if(!(this->flag&FLAG_NO_COLUMN_NAME)){
            for(int i1=0;i1<this->columnsType.size();i1++){
                this->columnsName.push_back(this->ser->getString());
            }
        }
        for(auto typ=this->columnsType.begin();typ!=this->columnsType.end();typ++){
            switch(*typ){
                case 'i':{
                    std::vector<TableCellValue> col;
                    auto col2=this->ser->getIntVec(this->rowCnt);
                    for(auto it=col2.begin();it!=col2.end();it++){
                        col.push_back({*it});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
                case 'l':{
                    std::vector<TableCellValue> col;
                    auto col2=this->ser->getLongVec(this->rowCnt);
                    for(auto it=col2.begin();it!=col2.end();it++){
                        col.push_back({*it});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
                case 'f':{
                    std::vector<TableCellValue> col;
                    auto col2=this->ser->getFloatVec(this->rowCnt);
                    for(auto it=col2.begin();it!=col2.end();it++){
                        col.push_back({*it});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
                case 'd':{
                    std::vector<TableCellValue> col;
                    auto col2=this->ser->getDoubleVec(this->rowCnt);
                    for(auto it=col2.begin();it!=col2.end();it++){
                        col.push_back({*it});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
                case 'b':{
                    std::vector<TableCellValue> col;
                    for(int i1=0;i1<rowCnt;i1++){
                        auto t1=this->ser->getBytes();
                        col.push_back({std::get<1>(t1),std::get<0>(t1)});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
                case 's':{
                    std::vector<TableCellValue> col;
                    for(int i1=0;i1<rowCnt;i1++){
                        col.push_back({this->ser->getString()});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
                case 'c':{
                    std::vector<TableCellValue> col;
                    for(int i1=0;i1<rowCnt;i1++){
                        col.push_back({this->ser->getVarint()!=0});
                    }
                    this->cols.push_back(std::move(col));
                }
                break;
            }
        }
        return this;
    }
    //Write data into bound serializer, create new Serializer if no bound. caller take response to delete the created Serializer.
    Serializer *buildSer(){
        if(this->ser==nullptr){
            this->ser=(new Serializer())->prepareSerializing(16);
        }
        int flag=0;
        if(this->columnsName.size()==0 && this->columnsType.size()>0){
            flag|=FLAG_NO_COLUMN_NAME;
        };
        this->ser->putVarint(flag);
        this->ser->putVarint(this->rowCnt);
        this->ser->putString(this->columnsType);
        if(!(flag & FLAG_NO_COLUMN_NAME)){
            for(auto name=this->columnsName.begin();name!=this->columnsName.end();name++){
                this->ser->putString(*name);
            }
        }
        for(int i1=0;i1<this->columnsType.length();i1++){
            switch(this->columnsType.at(i1)){
                case 'i':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putInt(it->i32);
                    }
                }
                break;
                case 'l':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putLong(it->i64);
                    }
                }
                break;
                case 'f':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putFloat(it->f32);
                    }
                }
                break;
                case 'd':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putDouble(it->f64);
                    }
                }
                break;
                case 'b':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putBytes(static_cast<uint8_t *>(it->bytes.base),it->bytes.length);
                    }
                }
                break;
                case 's':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putString(it->str);
                    }
                }
                break;
                case 'c':{
                    auto col=this->cols[i1];
                    for(auto it=col.begin();it!=col.end();it++){
                        this->ser->putVarint(it->bool2?1:0);
                    }
                }
                break;
            }
        }
        return this->ser;
    }
    
    TableSerializer *addRow(std::vector<TableCellValue>&& row){
        auto size=this->columnsType.size();
        for(int i1=0;i1<size;i1++){
            this->cols[i1].push_back(row[i1]);
        }
        this->rowCnt+=1;
        return this;
    }
    std::vector<TableCellValue> getRow(int32_t rowIndex){
        std::vector<TableCellValue> row;
        for(auto it=cols.begin();it!=cols.end();it++){
            row.push_back((*it)[rowIndex]);
        }
        return row;
    }
    int currentAddValuePosX=-1;
    protected:
    void moveToNextValuePosX(){
        currentAddValuePosX++;
        if(currentAddValuePosX>=cols.size()){
            currentAddValuePosX=0;
        }
        if(currentAddValuePosX==0){
            this->rowCnt++;
        }
    }
    public:
    //Add value to next cell, from left to right, from top to bottom.
    
    TableSerializer* addValue(int32_t v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({v});
        return this;
    }
    TableSerializer* addValue(int64_t v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({v});
        return this;
    }
    TableSerializer* addValue(float v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({v});
        return this;
    }
    TableSerializer* addValue(double v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({v});
        return this;
    }
    TableSerializer* addValue(std::tuple<int,uint8_t *> v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({std::get<1>(v),std::get<0>(v)});
        return this;
    }
    TableSerializer* addValue(std::string v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({v});
        return this;
    }
    TableSerializer* addValue(bool v){
        moveToNextValuePosX();
        this->cols[currentAddValuePosX].push_back({v});
        return this;
    }
    TableSerializer *setColumnsInfo(std::string columnsType,std::vector<std::string> columnsName){
        this->columnsType=columnsType;
        this->columnsName=columnsName;
        for(int i1=cols.size();i1<columnsType.size();i1++){
            cols.push_back({});
        }
        return this;
    }
    TableSerializer *setColumnsInfo(std::string columnsType){
        return this->setColumnsInfo(columnsType,std::vector<std::string>());
    }
    std::vector<std::string> getColumnsName(){
        return this->columnsName;
    }
    std::string getColumnsType(){
        return this->columnsType;
    }
    int getRowCount(){
        return this->rowCnt;
    }
    std::vector<std::unordered_map<std::string, TableCellValue>> toMapArray(){
        std::vector<std::unordered_map<std::string, TableCellValue>> r;
        for(auto rownum=0;rownum<getRowCount();rownum++){
            std::unordered_map<std::string, TableCellValue> maprow;
            for(auto cellnum=0;cellnum<cols.size();cellnum++){
                maprow.emplace(columnsName[cellnum],cols[cellnum][rownum]);
            }
            r.push_back(std::move(maprow));
        }
        return r;
    }
    //lack type info for fromMapArray, How to implement it?
    ~TableSerializer(){
    }
};


class NamedFunctionPPImpl1:public NamedFunctionPP{
    public:
    class AsyncReturn{
        public:
        PxpRequestWrap *req;
        std::function<void()> onReqFinished=[]()->void {};
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
            this->onReqFinished=[rawbuf]()->void {
                delete[] rawbuf;
            };
            req->nextStep();
        }
        //"ser" will be deleted by callee.
        void resolve(Serializer *ser){
            ser->buildPxpBytes(&req->result().bytes);
            this->onReqFinished=[ser,this]()->void {
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
        void resolve(const std::string& v){
            auto ser=new Serializer();
            ser->prepareSerializing(8)->putString(v);
            resolve(ser);
        }
        void resolve(PxpObject *obj){
            if(obj==nullptr){
                resolve((int32_t)-1);
            }else{
                int32_t refid=this->req->context().allocRef(obj);
                resolve(refid);
            }
        }
        void reject(const std::string &errorMessage){
            auto len=errorMessage.length();
            auto rawbuf=new uint8_t[len];
            req->result().bytes.base=rawbuf;
            req->result().bytes.length=len;
            req->result().next_part=nullptr;
            memmove(req->result().bytes.base,errorMessage.c_str(),len);
            req->onFinishPP=[rawbuf](PxpRequestWrap *req)->void {
                delete[] rawbuf;
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
        bool nextBool(){
            return this->asSerializer()->getVarint()!=0;
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
    std::function<void(Parameter *para,AsyncReturn *ret)> handler=[](auto a,auto b)->void {};
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
        r->onFinishPP=[para,ret](auto req)->void {
            ret->onReqFinished();
            delete para;
            delete ret;
        };
        this->handler(para,ret);
    }
    virtual ~NamedFunctionPPImpl1(){}
};

}