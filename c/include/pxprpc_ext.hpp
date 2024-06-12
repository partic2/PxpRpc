


#pragma once

#include "pxprpc_pp.hpp"

namespace pxprpc{

class TableSerializer{
    protected:
    Serializer *ser=nullptr;
    int32_t flag=0;
    int32_t rowCnt=0;
    std::string headerType;
    std::vector<std::string> headerName;
    std::vector<void *> cols;
    public:
    static const int FLAG_NO_HEADER_NAME=1;
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
        this->headerType=this->ser->getString();
        this->headerName.clear();
        if(!(this->flag&FLAG_NO_HEADER_NAME)){
            for(int i1=0;i1<this->headerType.size();i1++){
                this->headerName.push_back(this->ser->getString());
            }
        }
        for(auto typ=this->headerType.begin();typ!=this->headerType.end();typ++){
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
        if(this->headerName.size()==0 && this->headerType.size()>0){
            flag|=FLAG_NO_HEADER_NAME;
        };
        this->ser->putVarint(flag);
        this->ser->putVarint(this->rowCnt);
        this->ser->putString(this->headerType);
        if(!(flag & FLAG_NO_HEADER_NAME)){
            for(auto name=this->headerName.begin();name!=this->headerName.end();name++){
                this->ser->putString(*name);
            }
        }
        
        for(int i1=0;i1<this->headerType.length();i1++){
            switch(this->headerType.at(i1)){
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
        auto size=this->headerType.size();
        for(int i1=0;i1<size;i1++){
            switch(this->headerType.at(i1)){
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
    TableSerializer *setHeader(std::string headerType,std::vector<std::string> headerName){
        this->headerType=headerType;
        this->headerName=headerName;
        for(int i1=0;i1<headerType.size();i1++){
            switch(headerType.at(i1)){
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
    TableSerializer *setHeader(std::string headerType){
        return this->setHeader(headerType,std::vector<std::string>());
    }
    std::vector<std::string> getColumnsName(){
        return this->headerName;
    }
    std::string getColumnType(){
        return this->headerType;
    }
    int getRowCount(){
        return this->rowCnt;
    }
    ~TableSerializer(){
        for(int i1=0;i1<this->cols.size()&&i1<this->headerType.size();i1++){
            auto col=this->cols[i1];
            if(col!=nullptr){
                switch(this->headerType.at(i1)){
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

}