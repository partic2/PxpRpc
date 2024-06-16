

#include <jni.h>
#include <stdint.h>
#include <pthread.h>

jobject Java_pxprpc_pipe_NativeHelper_accessMemory(JNIEnv *env,jlong base,jint length){
    jobject nativeBuffer=(*env)->NewDirectByteBuffer(env,(void *)base,(jlong)length);
    return nativeBuffer;
}


jlong Java_pxprpc_pipe_NativeHelper_directBufferProperty(JNIEnv *env,jobject buf,int fieldId){
    switch(fieldId){
        case 0:
        return (jlong)(*env)->GetDirectBufferAddress(env,buf);
        case 1:
        return (jlong)(*env)->GetDirectBufferCapacity(env,buf);
    }
    return 0ll;
}

pthread_mutex_t eventMutex;
pthread_cond_t eventSignal;



jlong Java_pxprpc_pipe_NativeHelper_pullEvent(JNIEnv *env,jobject buf){
    void *base=(*env)->GetDirectBufferAddress(env,buf);
    int cap=(*env)->GetDirectBufferCapacity(env,buf);

}

jlong Java_pxprpc_pipe_NativeHelper_sendEvent(JNIEnv *env,uint64_t base,uint32_t cap){
    void *base1=(void *)base;

}



