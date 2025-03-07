
#include <pxprpc_pp.hpp>
#include <pxprpc_rtbridge_host.hpp>
#include <uv.h>
#include <memory>


namespace pxprpc_libuv {


template<typename RT>
void _uvcb_callDataAsFunction(RT *req){
  auto ptr=reinterpret_cast<std::function<void(void)>*>(req->data);
  if(ptr!=nullptr){
    (*ptr)();
    delete ptr;
    req->data=nullptr;
  }
  delete req;
}

void _uvcb_callDataAsFunction(uv_fs_t *req){
  auto ptr=reinterpret_cast<std::function<void(void)>*>(req->data);
  if(ptr!=nullptr){
    (*ptr)();
    delete ptr;
    req->data=nullptr;
  }
  uv_fs_req_cleanup(req);
  delete req;
}

class FileHandler:public pxprpc::PxpObject{
  public:
  uv_file fd=-1;
  bool opened=false;
  virtual ~FileHandler(){
    if(opened){
      auto req=new uv_fs_t();
      uv_fs_close(pxprpc_rtbridge_host::uvloop,req,fd,nullptr);
      delete req;
    }
  }
};

template <typename T>
inline void *voidPtr(T t){
  return reinterpret_cast<void*>(t);
}


void init() {
  //For convinience, Some api implement in sync mode.
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
            uv_fs_open(pxprpc_rtbridge_host::uvloop,req,path->c_str(),nflag,0666,&_uvcb_callDataAsFunction);
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
          uv_fs_read(pxprpc_rtbridge_host::uvloop,req,fh->fd,buf,1,offset,&_uvcb_callDataAsFunction);
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
          uv_fs_write(pxprpc_rtbridge_host::uvloop,req,fh->fd,buf,1,offset,&_uvcb_callDataAsFunction);
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
          uv_fs_unlink(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunction);
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
          uv_fs_mkdir(pxprpc_rtbridge_host::uvloop,req,path->c_str(),0666,&_uvcb_callDataAsFunction);
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
          uv_fs_rmdir(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunction);
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
          uv_fs_scandir(pxprpc_rtbridge_host::uvloop,req,path->c_str(),0,&_uvcb_callDataAsFunction);
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
            }
          });
          uv_fs_stat(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunction);
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
          uv_fs_rename(pxprpc_rtbridge_host::uvloop,req,path1->c_str(),path2->c_str(),&_uvcb_callDataAsFunction);
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
          uv_fs_ftruncate(pxprpc_rtbridge_host::uvloop,req,fh->fd,offset,&_uvcb_callDataAsFunction);
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
          uv_fs_readlink(pxprpc_rtbridge_host::uvloop,req,path->c_str(),&_uvcb_callDataAsFunction);
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
          uv_fs_chmod(pxprpc_rtbridge_host::uvloop,req,path->c_str(),mode,&_uvcb_callDataAsFunction);
        })
  );
}
} // namespace pxprpc_libuv
