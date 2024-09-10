
#include <pxprpc_rtbridge_host.hpp>

#include <jni.h>

extern "C"{
#include <pxprpc_rtbridge.h>
#include <pxprpc_pipe.h>
}

jobject Java_pxprpc_runtimebridge_NativeHelper_accessMemory(JNIEnv *env,jlong base,jint length){
    jobject nativeBuffer=env->NewDirectByteBuffer(reinterpret_cast<void *>(base),static_cast<jlong>(length));
    return nativeBuffer;
}


jlong Java_pxprpc_runtimebridge_NativeHelper_directBufferProperty(JNIEnv *env,jobject buf,int fieldId){
    switch(fieldId){
        case 0:
        return reinterpret_cast<jlong>(env->GetDirectBufferAddress(buf));
        case 1:
        return static_cast<jlong>(env->GetDirectBufferCapacity(buf));
    }
    return 0ll;
}


jint Java_pxprpc_runtimebridge_NativeHelper_pointerSize(JNIEnv *env){
    return sizeof(void *);
}


jlong Java_pxprpc_runtimebridge_NativeHelper_pipeConnect(JNIEnv *env,jobject servName){
    char *name=static_cast<char *>(env->GetDirectBufferAddress(servName));
    void *connection=pxprpc_rtbridge_pipe_connect(name);
    return (jlong)connection;
}

void Java_pxprpc_runtimebridge_NativeHelper_ioSend(JNIEnv *env,jlong io,jobject nativeBuffer,jobject errorString){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(&io);
    pxprpc_buffer_part part0;
    part0.bytes.base=env->GetDirectBufferAddress(nativeBuffer);
    part0.bytes.length=env->GetDirectBufferCapacity(nativeBuffer);
    part0.next_part=NULL;
    char *err=pxprpc_rtbridge_bsend(tio,&part0);
    if(err!=NULL){
        strncpy(static_cast<char *>(env->GetDirectBufferAddress(errorString)),err,env->GetDirectBufferCapacity(errorString));
    }
    return ;
}


jobject Java_pxprpc_runtimebridge_NativeHelper_ioReceive(JNIEnv *env,jlong io,jobject errorString){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(&io);
    pxprpc_buffer_part part0;
    part0.bytes.base=0;
    part0.bytes.length=0;
    part0.next_part=NULL;
    char *err=pxprpc_rtbridge_brecv(tio,&part0);
    if(err!=NULL){
        strncpy(static_cast<char *>(env->GetDirectBufferAddress(errorString)),err,env->GetDirectBufferCapacity(errorString));
        return NULL;
    }else{
        return env->NewDirectByteBuffer(part0.bytes.base,part0.bytes.length);
    }
}


void Java_pxprpc_runtimebridge_NativeHelper_ioBufFree(JNIEnv *env,jlong io,jobject buf){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(&io);
    tio->buf_free(env->GetDirectBufferAddress(buf));
}

void Java_pxprpc_runtimebridge_NativeHelper_ioClose(JNIEnv *env,jlong io,jobject buf){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(&io);
    tio->close(tio);
}

#include <uv.h>


void Java_pxprpc_runtimebridge_NativeHelper_ensureRtbInited(JNIEnv *env,jobject errorString){
    pxprpc_rtbridge_host::ensureInited();
}