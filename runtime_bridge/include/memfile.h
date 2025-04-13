

/*
   Pathed from https://github.com/COKPOWEHEU/memfile
*/

#ifndef __MEMFILE_H__
#define __MEMFILE_H__

#define MEMFILE_READ      0x01
#define MEMFILE_WRITE     0x02
#define MEMFILE_READWRITE (MEMFILE_READ | MEMFILE_WRITE)
#define MEMFILE_REWRITE   0x04
#define MEMFILE_FASTREWRITE 0x08


struct memfile_t{
  /* internal data */
  void *intdata;
  /* map result address. */
  void *data;
};

const char* memfile_create(const char filename[], unsigned char flags, unsigned long size, struct memfile_t *res);
const char* memfile_close(struct memfile_t *mem);

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

/*
 *  Linux
 */
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)
#include <sys/mman.h>
typedef struct{
  int fd;
  unsigned long size;
}memfile_d;

const char* memfile_create(const char filename[], unsigned char flags, unsigned long size, struct memfile_t *res){
  int fd;
  void *mem=NULL;
  int i, prot=0, acc=0;

  res->intdata = NULL;
  res->data = NULL;

  if(flags & MEMFILE_READ){
    prot |= PROT_READ;
    acc  |= S_IRUSR | S_IRGRP | S_IROTH;
    i = O_RDONLY;
  }
  if(flags & MEMFILE_WRITE){
    i = O_CREAT;
    prot |= PROT_WRITE;
    acc  |= S_IWUSR;
    if(flags & MEMFILE_READ){
      i |= O_RDWR;
    }else{
      i |= O_WRONLY;
    }
  }

  fd = open(filename, i, acc);
  if(fd < 0)return "open failed";
  if(flags & MEMFILE_WRITE){
    if(flags & MEMFILE_REWRITE){
      for(acc=0; acc<size; acc++)write(fd, " ", 1);
    }else if(flags & MEMFILE_FASTREWRITE){
      lseek(fd, size, SEEK_SET);
      write(fd, " ", 1);
    }
  }
  mem = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
  if(mem == NULL){
    close(fd);
    return "mmap failed";
  }

  res->intdata = malloc(sizeof(memfile_d));
  ((memfile_d*)(res->intdata))->fd = fd;
  ((memfile_d*)(res->intdata))->size = size;
  res->data = mem;
  
  return NULL;
}
const char* memfile_close(struct memfile_t *mem){
  int fd;
  unsigned long size;
  if(mem == NULL) NULL;
  if(mem->intdata == NULL)return NULL;
  fd = ((memfile_d*)(mem->intdata))->fd;
  size = ((memfile_d*)(mem->intdata))->size;
  munmap(mem->data, size);
  close(fd);
  free(mem->intdata);
  mem->intdata = NULL;
  mem->data = NULL;
  return NULL;
}
/*
 *  Win 32
 */
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
 //Win32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
typedef struct{
  HANDLE fd, fm;
}memfile_d;



const char* memfile_create(const char filename[], unsigned char flags, unsigned long size, struct memfile_t *res){
  HANDLE fd, fm;
  void *mem;
  int cflag=0, mflag=0, vflag=0;
  res->intdata = NULL;
  res->data = NULL;
  wchar_t *widename=NULL;
  int32_t widenamelen=0;
  int32_t utf8namelen=strlen(filename)+1;
  widenamelen=MultiByteToWideChar(CP_UTF8,0,filename,utf8namelen,NULL,0);
  if(widenamelen==0){
    return "MultiByteToWideChar failed";
  }
  widename=(wchar_t *)malloc(sizeof(wchar_t)*widenamelen);
  MultiByteToWideChar(CP_UTF8,0,filename,utf8namelen,widename,widenamelen);

  
  if(flags & MEMFILE_READ){
    cflag |= GENERIC_READ;
    mflag = PAGE_READONLY;
    vflag |= FILE_MAP_READ;
  }
  if(flags & MEMFILE_WRITE){
    cflag |= GENERIC_WRITE;
    mflag = PAGE_READWRITE;
    vflag |= FILE_MAP_WRITE;
  }
  
  if(flags & MEMFILE_WRITE){
    fd = CreateFileW(widename, cflag, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  }else{
    fd = CreateFileW(widename, cflag, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  }
  free(widename);
  
  
  if(fd == INVALID_HANDLE_VALUE)return "CreateFile failed";

  if(flags & MEMFILE_WRITE){
    if(flags & MEMFILE_REWRITE){
      char sample = ' ';
      long unsigned int cnt1=0;
      for(cflag=0; cflag<size;cflag++)WriteFile(fd, &sample, 1, &cnt1, NULL);
    }else if(flags & MEMFILE_FASTREWRITE){
      SetFilePointer(fd, size, NULL, FILE_BEGIN);
    }
  }
  
  fm = CreateFileMapping(fd, NULL, mflag, 0, size, NULL);
  if(fm == NULL){
    CloseHandle(fm);
    return "CreateFileMapping failed";
  }
  mem = MapViewOfFile(fm, vflag, 0, 0, size);
  if(mem == NULL){
    CloseHandle(fm);
    CloseHandle(fd);
    return "MapViewOfFile failed";
  }
  res->intdata = malloc(sizeof(memfile_d));
  ((memfile_d*)(res->intdata))->fd = fd;
  ((memfile_d*)(res->intdata))->fm = fm;
  res->data = mem;
  
  return NULL;
}
const char* memfile_close(struct memfile_t *mem){
  if(mem == NULL)return NULL;
  if(mem->intdata == NULL)return NULL;
  HANDLE fd, fm;
  fd = ((memfile_d*)(mem->intdata))->fd;
  fm = ((memfile_d*)(mem->intdata))->fm;
  UnmapViewOfFile(mem->data);
  CloseHandle(fm);
  CloseHandle(fd);
  free(mem->intdata);
  return NULL;
}
 /*
  *  Other systems (unsupported)
  */
#else
const char* memfile_create(const char filename[], unsigned char flags, unsigned long size, struct memfile_t *res){
  return "Not implemeted";
}
const char* memfile_close(struct memfile_t *mem){
  return "Not implemeted";
}
#endif

#endif