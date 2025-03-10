
#include <pxprpc_pp.hpp>
#include <pxprpc_rtbridge_host.hpp>
#include <uv.h>
#include <memory>


namespace pxprpc_libuv {


template<typename RT>
void _uvcb_callDataAsFunctionFreeReq(RT *req){
  auto ptr=reinterpret_cast<std::function<void(void)>*>(req->data);
  if(ptr!=nullptr){
    (*ptr)();
    delete ptr;
    req->data=nullptr;
  }
  delete req;
}

void _uvcb_callDataAsFunctionFreeReq(uv_fs_t *req){
  auto ptr=reinterpret_cast<std::function<void(void)>*>(req->data);
  if(ptr!=nullptr){
    (*ptr)();
    delete ptr;
    req->data=nullptr;
  }
  uv_fs_req_cleanup(req);
  delete req;
}


template<typename RT>
void _uvcb_callDataAsFunction(RT *handle){
  auto ptr=reinterpret_cast<std::function<void(void)>*>(handle->data);
  if(ptr!=nullptr){
    (*ptr)();
    delete ptr;
    handle->data=nullptr;
  }
}

template<typename RT>
void _uvcb_callDataAsFunctionWithStatFreeReq(RT* req, int status){
  auto ptr=reinterpret_cast<std::function<void(int)>*>(req->data);
  if(ptr!=nullptr){
    (*ptr)(status);
    delete ptr;
    req->data=nullptr;
  }
  delete req;
}

class FileHandler:public pxprpc::PxpObject{
  public:
  uv_file fd=-1;
  virtual ~FileHandler(){
    auto req=new uv_fs_t();
    if(fd>=0){
      uv_fs_close(pxprpc_rtbridge_host::uvloop,req,fd,nullptr);
    }
    delete req;
  }
};

void _UvStreamWrapAlloc(uv_handle_t* handle,size_t suggested_size,uv_buf_t* buf);
void _UvStreamWrapRead(uv_stream_t* stream,ssize_t nread,const uv_buf_t* buf);
void _UvStreamWrapConnection(uv_stream_t* server, int status);
class UvStreamWrap:public pxprpc::PxpObject{
  public:
  uv_stream_t *stream;
  std::vector<char> readBuf={};
  std::deque<pxprpc::NamedFunctionPPImpl1::AsyncReturn *> readWaiting={};
  std::deque<pxprpc::NamedFunctionPPImpl1::AsyncReturn *> acceptWaiting={};
  int8_t closeOnDestruct=0;
  int8_t listening=0;

  int32_t errcode=0;
  
  void pushError(int errcode){
    this->errcode=errcode;
    processing();
  }
  void pushData(char *buf,int len){
    if(len==0)return;
    if(readBuf.size()>1024*1024){
      //Too large buffer. discard
      return;
    }
    std::copy(buf,buf+len,std::back_inserter(readBuf));
    processing();
  }
  void processing(){
    if(readWaiting.size()>0 && readBuf.size()>0){
      readWaiting.front()->resolve(readBuf.data(),readBuf.size());
      readWaiting.pop_front();
      readBuf.clear();
    }
    if(errcode<0){
      while(readWaiting.size()>0){
        readWaiting.front()->reject(uv_err_name(errcode));
        readWaiting.pop_front();
      }
      while(acceptWaiting.size()>0){
        acceptWaiting.front()->reject(uv_err_name(errcode));
        acceptWaiting.pop_front();
      }
    }
  }
  virtual int startReading(){
    stream->data=this;
    return uv_read_start(stream,_UvStreamWrapAlloc,_UvStreamWrapRead);
  }
  virtual int startListening(){
    stream->data=this;
    return uv_listen(stream,16,&_UvStreamWrapConnection);
  }
  virtual void init(){
    //Implemented in subclass;
  }
  virtual UvStreamWrap *createStream(){
    //Implemented in subclass;
    return nullptr;
  }
  virtual void pushConn(UvStreamWrap *stream){
    //No connection queue yet, maybe add in future?
    if(acceptWaiting.size()>0){
      acceptWaiting.front()->resolve(stream);
      acceptWaiting.pop_front();
    }
  }
  virtual void pullData(pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret){
    readWaiting.push_back(ret);
    processing();
  }
  virtual void acceptConn(pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret){
    acceptWaiting.push_back(ret);
    processing();
  }
  virtual void writeData(uint8_t *buf,int32_t size,pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret){
    if(errcode<0)ret->reject(uv_err_name(errcode));
    auto bufs=new uv_buf_t[1];
    bufs[0].base=(char *)buf;
    bufs->len=size;
    auto writeReq=new uv_write_t();
    //uv_try_write always failed on windows pipe, So don't use uv_write instead.
    writeReq->data=new std::function<void(int)>([ret,bufs](int stat)->void {
      delete[] bufs;
      if(stat<0){
        ret->reject(uv_err_name(stat));
      }else{
        ret->resolve(stat);
      }
    });
    uv_write(writeReq,stream,bufs,1,_uvcb_callDataAsFunctionWithStatFreeReq);
  }
  
  virtual ~UvStreamWrap(){
    if(closeOnDestruct){
      uv_read_stop(stream);
      this->stream->data=nullptr;
      uv_close((uv_handle_t *)this->stream,_uvcb_callDataAsFunctionFreeReq);
    }
  }
};

void _UvStreamWrapAlloc(uv_handle_t* handle,size_t suggested_size,uv_buf_t* buf){
  buf->base=new char[suggested_size];
  buf->len=suggested_size;
};
void _UvStreamWrapRead(uv_stream_t* stream,ssize_t nread,const uv_buf_t* buf){
  auto wrap=(UvStreamWrap *)(stream->data);
  if(wrap==nullptr)return;
  wrap->pushData(buf->base,nread);
  delete[] buf->base;
};
void _UvStreamWrapConnection(uv_stream_t* server, int status){
  auto wrap=(UvStreamWrap *)(server->data);
  if(wrap==nullptr)return;
  if(status==0){
    auto newConn=wrap->createStream();
    newConn->init();
    uv_accept(server,newConn->stream);
    newConn->closeOnDestruct=1;
    newConn->startReading();
    wrap->pushConn(newConn);
  }else{
    wrap->pushError(status);
  }
  
}

void _uvProcessExit(uv_process_t* proc, int64_t exit_status, int term_signal);
class UvProcessWrap:public pxprpc::PxpObject{
  public:
  //uv_process_t can only delete after process stopped and process closed
  uv_process_t *proc=nullptr;
  uv_stdio_container_s stdio[3]={};
  bool alive=false;
  int64_t exitStatus=0;
  int termSignal=0;
  std::vector<pxprpc::NamedFunctionPPImpl1::AsyncReturn *> onProcExit;
  UvProcessWrap(){
  }
  void processEvent(){
    if(!alive){
      for(auto it=onProcExit.begin();it!=onProcExit.end();it++){
        (*it)->resolve((new pxprpc::Serializer())->prepareSerializing(16)->
          putVarint(alive?1:0)->putLong(exitStatus)->putInt(termSignal));
      }
      onProcExit.clear();
    }
  }
  void getProcessResult(bool waitExit,pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret){
    if(waitExit && alive){
      onProcExit.push_back(ret);
    }else{
      ret->resolve((new pxprpc::Serializer())->prepareSerializing(16)->
          putVarint(alive?1:0)->putLong(exitStatus)->putInt(termSignal));
    }
  }
  void initStdio(){
    for(int i1=0;i1<3;i1++){
      (int &)stdio[i1].flags=UV_CREATE_PIPE|UV_READABLE_PIPE|UV_WRITABLE_PIPE;
      auto p1=new uv_pipe_t();
      stdio[i1].data.stream=(uv_stream_t *)p1;
      uv_pipe_init(pxprpc_rtbridge_host::uvloop,p1,0);
    }
  }
  virtual ~UvProcessWrap(){
    if(proc!=nullptr){
      proc->data=nullptr;
    }
    for(int i1=0;i1<3;i1++){
      if(stdio[i1].data.stream!=nullptr){
        stdio[i1].data.stream->data=nullptr;
        uv_read_stop(stdio[i1].data.stream);
        uv_close((uv_handle_t *)stdio[i1].data.stream,_uvcb_callDataAsFunctionFreeReq);
        stdio[i1].data.stream=nullptr;
      }
    }
    
  }

  int spawn(std::string& fileName,std::vector<std::string>& args,std::string& cwd,std::vector<std::string>& envs){
    uv_process_options_s opt={};
    memset(&opt,0,sizeof(opt));
    
    if(fileName!=""){
      opt.file=fileName.c_str();
    }else{
      opt.file=args[0].c_str();
    }
    if(cwd!=""){
      opt.cwd=cwd.c_str();
    }
    opt.stdio_count=3;
    initStdio();
    opt.stdio=this->stdio;

    auto args2=new char*[args.size()+1];
    for(int i1=0;i1<args.size();i1++){
      args2[i1]=const_cast<char *>(args[i1].c_str());
    }
    args2[args.size()]=NULL;
    opt.args=args2;

    auto envs2=new char*[envs.size()+1];
    if(envs.size()>0){
      for(int i1=0;i1<envs.size();i1++){
        envs2[i1]=const_cast<char *>(envs[i1].c_str());
      }
      envs2[envs.size()]=NULL;
      opt.env=envs2;
    }

    opt.exit_cb=_uvProcessExit;
    proc=new uv_process_t();
    proc->data=this;
    int r=uv_spawn(pxprpc_rtbridge_host::uvloop,proc,&opt);
    delete[] args2;
    delete[] envs2;
    return r;
  }
  UvStreamWrap *getStdio(int index){
    auto sw=new UvStreamWrap();
    sw->stream=stdio[index].data.stream;
    sw->startReading();
    return sw;
  }
};

void _uvProcessExit(uv_process_t* proc, int64_t exit_status, int term_signal){
  auto handle=reinterpret_cast<UvProcessWrap *>(proc->data);
  if(handle!=nullptr){
    handle->alive=false;
    handle->exitStatus=exit_status;
    handle->termSignal=term_signal;
    handle->processEvent();
    handle->proc=nullptr;
  }
  proc->data=nullptr;
  uv_close((uv_handle_t *)proc,_uvcb_callDataAsFunctionFreeReq);
}


class UvPipeWrap:public UvStreamWrap{
  public:
  uv_pipe_t* pipe(){
    return reinterpret_cast<uv_pipe_t *>(stream);
  }
  virtual void init() override{
    stream=(uv_stream_t *)new uv_pipe_t();
    uv_pipe_init(pxprpc_rtbridge_host::uvloop,pipe(),0);
    closeOnDestruct=1;
  }
  virtual UvStreamWrap *createStream() override{
    return new UvPipeWrap();
  }
  int bind(std::string path){
    return uv_pipe_bind(pipe(),path.c_str());
  }
  void connect(const char* pipeName,std::function<void(int)> cb){
    auto connReq=new uv_connect_t();
    connReq->data=new decltype(cb)(cb);
    uv_pipe_connect(connReq,pipe(),pipeName,&_uvcb_callDataAsFunctionWithStatFreeReq);
  }
};

class UvTcpWrap:public UvStreamWrap{
  public:
  uv_tcp_t* tcp(){
    return reinterpret_cast<uv_tcp_t *>(stream);
  }
  virtual void init() override{
    stream=(uv_stream_t *)new uv_tcp_t();
    uv_tcp_init(pxprpc_rtbridge_host::uvloop,tcp());
    closeOnDestruct=1;
  }
  virtual UvStreamWrap *createStream() override{
    return new UvTcpWrap();
  }
  int bind(std::string ip,int32_t port){
    union{
      sockaddr_in v4;
      sockaddr_in6 v6;
    } addrin;
    int err=uv_ip4_addr(ip.c_str(),port,&addrin.v4);
    if(err<0){
      err=uv_ip6_addr(ip.c_str(),port,&addrin.v6);
      if(err<0){
        return err;
      }
    }
    return uv_tcp_bind(tcp(),reinterpret_cast<const sockaddr *>(&addrin),0);
  }
  union{
      sockaddr_in v4;
      sockaddr_in6 v6;
    } addrin={};
  int connect(const char* ip,int32_t port,std::function<void(int)> cb){
    int err=uv_ip4_addr(ip,port,&addrin.v4);
    if(err<0){
      err=uv_ip6_addr(ip,port,&addrin.v6);
      if(err<0){
        return err;
      }
    }
    auto connReq=new uv_connect_t();
    connReq->data=new decltype(cb)(cb);
    return uv_tcp_connect(connReq,tcp(),reinterpret_cast<const sockaddr *>(&addrin),_uvcb_callDataAsFunctionWithStatFreeReq);
  }
};

void _UvUdpWrapRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);

class UvUdpWrap:public pxprpc::PxpObject{
  public:
  uv_udp_t *udp=new uv_udp_t();
  pxprpc::Serializer *readBuf=nullptr;
  std::deque<pxprpc::NamedFunctionPPImpl1::AsyncReturn *> readWaiting={};
  int8_t closeOnDestruct=0;
  int8_t listening=0;

  int32_t errcode=0;
  
  void pushError(int errcode){
    this->errcode=errcode;
    processing();
  }
  int bind(const struct sockaddr* addr){
    uv_udp_init(pxprpc_rtbridge_host::uvloop,udp);
    return uv_udp_bind(udp,addr,0);
  }
  void pushData(char *buf,int len,const struct sockaddr* addr){
    if(len==0)return;
    if(readBuf==nullptr){
      readBuf=new pxprpc::Serializer();
      readBuf->prepareSerializing(32);
    }
    if(readBuf->wrapped.pos>1024*1024){
      //Too large buffer. discard.
      return;
    }
    readBuf->putBytes((uint8_t *)addr,sizeof(sockaddr_storage));
    readBuf->putBytes((uint8_t *)buf,len);
    processing();
  }
  void processing(){
    if(readWaiting.size()>0 && readBuf->wrapped.pos>0){
      //Zero length means END
      readBuf->putBytes(nullptr,0);
      readWaiting.front()->resolve(readBuf);
      readWaiting.pop_front();
      //Serializer free by pxprpc;
      readBuf=nullptr;
    }
    if(errcode<0){
      while(readWaiting.size()>0){
        readWaiting.front()->reject(uv_err_name(errcode));
        readWaiting.pop_front();
      }
    }
  }
  virtual int startReading(){
    udp->data=this;
    return uv_udp_recv_start(udp,_UvStreamWrapAlloc,_UvUdpWrapRead);
  }
  virtual void init(){
  }
  void pullData(pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret){
    readWaiting.push_back(ret);
    processing();
  }
  void writeData(uint8_t *buf,int32_t size,std::tuple<int32_t,uint8_t *> addr,pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret){
    if(errcode<0)ret->reject(uv_err_name(errcode));
    auto bufs=new uv_buf_t[1];
    bufs[0].base=(char *)buf;
    bufs->len=size;
    auto writeReq=new uv_udp_send_t();
    //copy addr to avoid alignment issue.
    auto addr2=new sockaddr_storage();
    memmove(addr2,std::get<1>(addr),std::min((int32_t)sizeof(sockaddr_storage),std::get<0>(addr)));
    writeReq->data=new std::function<void(int)>([ret,bufs,addr2](int stat)->void {
      delete[] bufs;
      delete addr2;
      if(stat<0){
        ret->reject(uv_err_name(stat));
      }else{
        ret->resolve(stat);
      }
    });
    uv_udp_send(writeReq,udp,bufs,1,reinterpret_cast<const sockaddr *>(addr2),_uvcb_callDataAsFunctionWithStatFreeReq);
  }
  
  virtual ~UvUdpWrap(){
    if(closeOnDestruct){
      uv_udp_recv_stop(udp);
      this->udp->data=nullptr;
      uv_close((uv_handle_t *)this->udp,_uvcb_callDataAsFunctionFreeReq);
    }
    if(readBuf!=nullptr){
      delete readBuf;
    }
  }
};

void _UvUdpWrapRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags){
  auto wrap=(UvUdpWrap *)(handle->data);
  if(wrap==nullptr)return;
  wrap->pushData(buf->base,nread,addr);
  delete[] buf->base;
};


std::tuple<std::string,int32_t> _addrinToString(sockaddr_storage *addr){
  auto namestr=new char[256];
  int port=0;
  if(addr->ss_family==AF_INET){
    uv_ip4_name(reinterpret_cast<sockaddr_in *>(addr),namestr,256);
    port=reinterpret_cast<sockaddr_in *>(addr)->sin_port;
  }else if(addr->ss_family==AF_INET6){
    uv_ip6_name(reinterpret_cast<sockaddr_in6 *>(addr),namestr,256);
    port=reinterpret_cast<sockaddr_in6 *>(addr)->sin6_port;
  }else{
    delete[] namestr;
    return std::tuple("ERR:Unknown SS_FAMILY",-1);
  }
  auto r=std::tuple(std::string(namestr),port);
  delete[] namestr;
  return r;
}



void init() {
  pxprpc::defaultFuncMap.add(
      (new pxprpc::NamedFunctionPPImpl1())
          ->init("pxprpc_libuv.fs_open", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
            auto path = std::make_shared<std::string>(para->nextString());
            std::string flag = para->nextString();
            int nflag = 0;
            if (flag == "r") {
              nflag |= O_RDONLY;
            } else if (flag == "w") {
              nflag |= O_RDWR | O_CREAT;
            } else if (flag == "r+") {
              nflag |= O_RDWR;
            }
            auto req=new uv_fs_t();
            req->data=new std::function<void(void)>([req,para,ret,path]()-> void {
              if(req->result>=0){
                auto fh = new FileHandler();
                fh->fd=req->result;
                ret->resolve(fh);
              }else{
                ret->reject(uv_err_name(req->result));
              }
              uv_fs_req_cleanup(req);
            });
            uv_fs_open(pxprpc_rtbridge_host::uvloop,req,path->c_str(),nflag,0666,&_uvcb_callDataAsFunctionFreeReq);
          })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_read", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto fh=static_cast<FileHandler *>(para->nextObject());
          auto size=para->nextInt();
          auto offset=para->nextLong();
          auto req=new uv_fs_t();
          auto buf=new uv_buf_t[1];
          buf[0].base=new char[size];
          buf[0].len=size;
          req->data=new std::function<void()>([req,buf,ret]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve((void *)buf[0].base,req->result);
            }
            delete[] buf[0].base;
            delete[] buf;
          });
          uv_fs_read(pxprpc_rtbridge_host::uvloop,req,fh->fd,buf,1,offset,&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_write", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto fh=static_cast<FileHandler *>(para->nextObject());
          auto offset=para->nextLong();
          auto buf2=para->nextBytes();
          auto req=new uv_fs_t();
          auto buf=new uv_buf_t[1];
          buf[0].base=(char *)std::get<1>(buf2);
          buf[0].len=std::get<0>(buf2);
          req->data=new std::function<void()>([req,buf,ret]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve(req->result);
            }
            delete[] buf;
          });
          uv_fs_write(pxprpc_rtbridge_host::uvloop,req,fh->fd,buf,1,offset,&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_unlink", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve();
            }
          });
          uv_fs_unlink(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunctionFreeReq);
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_mkdir", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve();
            }
          });
          uv_fs_mkdir(pxprpc_rtbridge_host::uvloop,req,path->c_str(),0666,&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_rmdir", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve();
            }
          });
          uv_fs_rmdir(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_scandir", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          auto req=new uv_fs_t();
          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              pxprpc::TableSerializer tabser;
              tabser.setColumnInfo("ss",std::vector<std::string>({"name","type"}));
              uv_dirent_t ent;
              while(uv_fs_scandir_next(req,&ent)!=UV_EOF){
                tabser.addValue(ent.name);
                switch(ent.type){
                  case UV_DIRENT_FILE:
                  tabser.addValue("file");
                  break;
                  case UV_DIRENT_DIR:
                  tabser.addValue("dir");
                  break;
                  case UV_DIRENT_LINK:
                  tabser.addValue("link");
                  break;
                  default:
                  tabser.addValue("unknown");
                  break;
                }
              }
              ret->resolve(tabser.buildSer());
            }
          });
          uv_fs_scandir(pxprpc_rtbridge_host::uvloop,req,path->c_str(),0,&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_stat", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              pxprpc::TableSerializer tabser;
              tabser.setColumnInfo("lsli",std::vector<std::string>({"size","type","mtime","mode"}));
              tabser.addValue((int64_t)req->statbuf.st_size);
              if(req->statbuf.st_mode & S_IFREG){
                tabser.addValue("file");
              }else if(req->statbuf.st_mode & S_IFDIR){
                tabser.addValue("dir");
              }else if(req->statbuf.st_mode & S_IFLNK){
                tabser.addValue("link");
              }else{
                tabser.addValue("unknown");
              }
              tabser.addValue(req->statbuf.st_mtim.tv_sec*1000ll+req->statbuf.st_mtim.tv_nsec/1000000ll);
              tabser.addValue((int32_t)(req->statbuf.st_mode&0x7fffffff));
              ret->resolve(tabser.buildSer());
            }
          });
          uv_fs_stat(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_rename", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path1=std::make_shared<std::string>(para->nextString());
          auto path2=std::make_shared<std::string>(para->nextString());
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path1,path2]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve();
            }
          });
          uv_fs_rename(pxprpc_rtbridge_host::uvloop,req,path1->c_str(),path2->c_str(),&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_ftruncate", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto fh=static_cast<FileHandler *>(para->nextObject());
          auto offset=para->nextLong();
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve();
            }
          });
          uv_fs_ftruncate(pxprpc_rtbridge_host::uvloop,req,fh->fd,offset,&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_readlink", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve((char *)req->ptr);
            }
          });
          uv_fs_readlink(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunctionFreeReq);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.fs_chmod", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto path=std::make_shared<std::string>(para->nextString());
          auto mode=para->nextInt();
          auto req=new uv_fs_t();

          req->data=new std::function<void()>([req,ret,path]()->void {
            if(req->result<0){
              ret->reject(uv_err_name(req->result));
            }else{
              ret->resolve();
            }
          });
          uv_fs_chmod(pxprpc_rtbridge_host::uvloop,req,path->c_str(),mode,&_uvcb_callDataAsFunctionFreeReq);
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.stream_read", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto stream=static_cast<UvStreamWrap *>(para->nextObject());
          stream->pullData(ret);
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.stream_write", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto stream=static_cast<UvStreamWrap *>(para->nextObject());
          auto data=para->nextBytes();
          stream->writeData(std::get<1>(data),std::get<0>(data),ret);
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.stream_accept", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto stream=static_cast<UvStreamWrap *>(para->nextObject());
          stream->acceptConn(ret);
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.spawn", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          UvProcessWrap *p=new UvProcessWrap();
          std::vector<std::string> args;
          std::vector<std::string> envs;

          auto fileName=para->nextString();
          auto cwd=para->nextString();

          int argsCount=para->nextInt();
          for(int i1=0;i1<argsCount;i1++){
            args.push_back(para->nextString());
          }
          int envsCount=para->nextInt();
          for(int i1=0;i1<envsCount;i1++){
            envs.push_back(para->nextString());
          }
          int r=p->spawn(fileName,args,cwd,envs);
          if(r<0){
            ret->reject(uv_err_name(r));
            delete p;
          }else{
            ret->resolve(p);
          }
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.process_stdio", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto proc=reinterpret_cast<UvProcessWrap *>(para->nextObject());
          auto index=para->nextInt();
          ret->resolve(proc->getStdio(index));
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.process_get_result", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto proc=reinterpret_cast<UvProcessWrap *>(para->nextObject());
          proc->getProcessResult(para->asSerializer()->getVarint(),ret);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.pipe_bind", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto pipe=new UvPipeWrap();
          pipe->init();
          int err=pipe->bind(para->nextString());
          if(err<0){
            ret->reject(uv_err_name(err));
            delete pipe;
          }else{
            pipe->startListening();
            ret->resolve(pipe);
          }
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.pipe_connect", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto pipe=new UvPipeWrap();
          pipe->init();
          auto name=std::make_shared<std::string>(para->nextString());
          pipe->connect(name->c_str(),[name,ret,pipe](int stat)->void {
            if(stat<0){
              ret->reject(uv_err_name(stat));
              delete pipe;
            }else{
              pipe->startReading();
              ret->resolve(pipe);
            }
          });
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.tcp_bind", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto ip=para->nextString();
          auto port=para->nextInt();
          auto tcp=new UvTcpWrap();
          tcp->init();
          int err=tcp->bind(ip,port);
          if(err<0){
            ret->reject(uv_err_name(err));
            delete tcp;
          }else{
            tcp->startListening();
            ret->resolve(tcp);
          }
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.tcp_connect", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto ip=std::make_shared<std::string>(para->nextString());
          auto port=para->nextInt();
          auto tcp=new UvTcpWrap();
          tcp->init();
          tcp->connect(ip->c_str(),port,[ip,ret,tcp](int stat)->void {
            if(stat<0){
              ret->reject(uv_err_name(stat));
              delete tcp;
            }else{
              tcp->startReading();
              ret->resolve(tcp);
            }
          });
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.tcp_getpeername", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto tcp=reinterpret_cast<UvTcpWrap *>(para->nextObject());
          sockaddr_storage addr;
          int namelen=sizeof(sockaddr_storage);
          int err=uv_tcp_getpeername(tcp->tcp(),reinterpret_cast<sockaddr *>(&addr),&namelen);
          if(err<0){
            ret->reject(uv_err_name(err));
            return;
          }
          auto r=_addrinToString(&addr);
          auto ser=new pxprpc::Serializer();
          ser->putString(std::get<0>(r))->putInt(std::get<1>(r));
          ret->resolve(ser);
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.interface_address", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          uv_interface_address_t *addrs;
          int count=0;
          int err=uv_interface_addresses(&addrs,&count);
          if(err<0){
            ret->reject(uv_err_name(err));
            return;
          }
          pxprpc::TableSerializer table;
          table.setColumnInfo("sbss",std::vector<std::string>({"name","phys_addr","ip","netmask"}));
          for(int i1=0;i1<count;i1++){
            table.addValue(addrs[i1].name);
            table.addValue(std::tuple(6,(uint8_t *)addrs[i1].phys_addr));
            table.addValue(std::get<0>(_addrinToString((sockaddr_storage *)&addrs[i1].address)));
            table.addValue(std::get<0>(_addrinToString((sockaddr_storage *)&addrs[i1].netmask)));
          }
          ret->resolve(table.buildSer());
          uv_free_interface_addresses(addrs,count);
        })
  );
  
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.os_getenv", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto name=para->nextString();
          //Windows DO NOT support write back require length. So always use fix length buffer now.
          size_t len=4096;
          auto buf=new char[len];
          int r=uv_os_getenv(name.c_str(),buf,&len);
          if(r<0){
            ret->reject(uv_err_name(r));
          }else{
            ret->resolve(buf);
          }
          delete[] buf;
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.os_setenv", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto name=para->nextString();
          auto val=para->nextString();
          int r=uv_os_setenv(name.c_str(),val.c_str());
          if(r<0){
            ret->reject(uv_err_name(r));
          }else{
            ret->resolve();
          }
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.os_unsetenv", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto name=para->nextString();
          int r=uv_os_unsetenv(name.c_str());
          if(r<0){
            ret->reject(uv_err_name(r));
          }else{
            ret->resolve();
          }
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.os_getprop", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto name=para->nextString();
          size_t len=4096;
          if(name=="cwd"){
            auto buf=new char[len];
            int r=uv_cwd(buf,&len);
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve(buf);
            }
            delete[] buf;
          }else if(name=="exepath"){
            auto buf=new char[len];
            int r=uv_exepath(buf,&len);
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve(buf);
            }
            delete[] buf;
          }else if(name=="tmpdir"){
            auto buf=new char[len];
            int r=uv_os_tmpdir(buf,&len);
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve(buf);
            }
            delete[] buf;
          }else if(name=="hostname"){
            auto buf=new char[len];
            int r=uv_os_gethostname(buf,&len);
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve(buf);
            }
            delete[] buf;
          }else if(name=="process_title"){
            len=256;
            auto buf=new char[len];
            int r=uv_get_process_title(buf,len);
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve(buf);
            }
            delete[] buf;
          }else if(name=="uname"){
            uv_utsname_t buffer;
            int r=uv_os_uname(&buffer);
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve(std::string(buffer.sysname)+"\n"+
                          std::string(buffer.release)+" "+std::string(buffer.version)+"\n"+
                          std::string(buffer.machine));
            }
          }else{
            ret->reject("Unsupported property");
          }
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.os_setprop", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto name=para->nextString();
          auto val=para->nextString();
          if(name=="cwd"){
            int r=uv_chdir(val.c_str());
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve();
            }
          }else if(name=="process_title"){
            int r=uv_set_process_title(val.c_str());
            if(r<0){
              ret->reject(uv_err_name(r));
            }else{
              ret->resolve();
            }
          }else{
            ret->reject("Unsupported property");
          }
        })
  );

  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.get_memory_info", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto tabser=new pxprpc::TableSerializer();
          tabser->setColumnInfo("lll",std::vector<std::string>({"free","total","available"}));
          int r=0;
          tabser->addValue((int64_t)uv_get_free_memory());
          tabser->addValue((int64_t)uv_get_total_memory());
          tabser->addValue((int64_t)uv_get_available_memory());
          if(r<0){
            ret->reject(uv_err_name(r));
          }else{
            ret->resolve(tabser->buildSer());
          }
          delete tabser;
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.gettimeofday", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          uv_timeval64_t tim;
          int r=uv_gettimeofday(&tim);
          if(r<0){
            ret->reject(uv_err_name(r));
          }else{
            ret->resolve(tim.tv_sec*1000+tim.tv_sec/1000000);
          }
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.udp_bind", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto addr=para->asRaw();
          auto udp=new UvUdpWrap();
          int r=udp->bind((const struct sockaddr *)addr);
          if(r<0){
            ret->reject(uv_err_name(r));
            delete udp;
          }else{
            ret->resolve(udp);
          }
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.udp_send", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto udp=reinterpret_cast<UvUdpWrap *>(para->nextObject());
          auto addr=para->nextBytes();
          auto data=para->nextBytes();
          udp->writeData(std::get<1>(data),std::get<0>(data),addr,ret);
        })
  );
  pxprpc::defaultFuncMap.add(
    (new pxprpc::NamedFunctionPPImpl1())
        ->init("pxprpc_libuv.udp_recv", [](pxprpc::NamedFunctionPPImpl1::Parameter *para, pxprpc::NamedFunctionPPImpl1::AsyncReturn *ret) -> void {
          auto udp=static_cast<UvUdpWrap *>(para->nextObject());
          udp->pullData(ret);
        })
  );
}
} // namespace pxprpc_libuv
