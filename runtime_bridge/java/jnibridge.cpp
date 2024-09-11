
#include <pxprpc_rtbridge_host.hpp>

#include <jni.h>

extern "C"{
#include <pxprpc_rtbridge.h>
#include <pxprpc_pipe.h>
#include <uv.h>

#ifdef __GNUC__
//For GCC, symbols are export by default
#undef JNIEXPORT
#define JNIEXPORT
#endif
/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    accessMemory
 * Signature: (JI)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_pxprpc_runtimebridge_NativeHelper_accessMemory
  (JNIEnv *, jclass, jlong, jint);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    directBufferProperty
 * Signature: (Ljava/nio/ByteBuffer;I)J
 */
JNIEXPORT jlong JNICALL Java_pxprpc_runtimebridge_NativeHelper_directBufferProperty
  (JNIEnv *, jclass, jobject, jint);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    pointerSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_pxprpc_runtimebridge_NativeHelper_pointerSize
  (JNIEnv *, jclass);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    pipeConnect
 * Signature: (Ljava/nio/ByteBuffer;)J
 */
JNIEXPORT jlong JNICALL Java_pxprpc_runtimebridge_NativeHelper_pipeConnect
  (JNIEnv *, jclass, jobject);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    ioSend
 * Signature: (JLjava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioSend
  (JNIEnv *, jclass, jlong, jobject, jobject);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    ioReceive
 * Signature: (JLjava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioReceive
  (JNIEnv *, jclass, jlong, jobject);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    ioBufFree
 * Signature: (JLjava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioBufFree
  (JNIEnv *, jclass, jlong, jobject);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    ioClose
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioClose
  (JNIEnv *, jclass, jlong);

/*
 * Class:     pxprpc_runtimebridge_NativeHelper
 * Method:    ensureRtbInited
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ensureRtbInited
  (JNIEnv *, jclass, jobject);



JNIEXPORT jobject JNICALL Java_pxprpc_runtimebridge_NativeHelper_accessMemory
(JNIEnv *env,jclass clazz,jlong base,jint length){
    jobject nativeBuffer=env->NewDirectByteBuffer(reinterpret_cast<void *>(base),static_cast<jlong>(length));
    return nativeBuffer;
}


JNIEXPORT jlong JNICALL Java_pxprpc_runtimebridge_NativeHelper_directBufferProperty
(JNIEnv *env,jclass clazz,jobject buf,jint fieldId){
    switch(fieldId){
        case 0:
        return reinterpret_cast<jlong>(env->GetDirectBufferAddress(buf));
        case 1:
        return static_cast<jlong>(env->GetDirectBufferCapacity(buf));
    }
    return 0ll;
}


JNIEXPORT jint JNICALL Java_pxprpc_runtimebridge_NativeHelper_pointerSize
(JNIEnv *env,jclass clazz){
    return sizeof(void *);
}


JNIEXPORT jlong JNICALL Java_pxprpc_runtimebridge_NativeHelper_pipeConnect
(JNIEnv *env,jclass clazz,jobject servName){
    char *name=static_cast<char *>(env->GetDirectBufferAddress(servName));
    void *connection=pxprpc_rtbridge_pipe_connect(name);
    return (jlong)connection;
}

JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioSend
(JNIEnv *env,jclass clazz,jlong io,jobject nativeBuffer,jobject errorString){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(io);
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


JNIEXPORT jobject JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioReceive
(JNIEnv *env,jclass clazz,jlong io,jobject errorString){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(io);
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


JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioBufFree
(JNIEnv *env,jclass clazz,jlong io,jobject buf){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(io);
    tio->buf_free(env->GetDirectBufferAddress(buf));
}

JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ioClose
(JNIEnv *env,jclass clazz,jlong io){
    struct pxprpc_abstract_io *tio=reinterpret_cast<struct pxprpc_abstract_io *>(io);
    tio->close(tio);
}



JNIEXPORT void JNICALL Java_pxprpc_runtimebridge_NativeHelper_ensureRtbInited
(JNIEnv *env,jclass clazz,jobject errorString){
    pxprpc_rtbridge_host::ensureInited();
}

}