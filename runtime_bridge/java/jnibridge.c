

#include <jni.h>

#include "pxprpc_rtbridge.h"

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


jint Java_pxprpc_pipe_NativeHelper_pointerSize(JNIEnv *env){
    return sizeof(void *);
}


jobject Java_pxprpc_pipe_NativeHelper_pullResult(JNIEnv *env,jlong rtb){
    pxprpc_rtbridge_context c=(pxprpc_rtbridge_context)rtb;
    struct pxprpc_rtbridge_event_s  *evt=pxprpc_rtbridge_pull_result(c);
    return (*env)->NewDirectByteBuffer(env,evt,evt->eventSize);
}


void Java_pxprpc_pipe_NativeHelper_pushInvoc(JNIEnv *env,jlong rtb,jobject jevtbuf){
    pxprpc_rtbridge_context c=(pxprpc_rtbridge_context)rtb;
    int len=(*env)->GetDirectBufferCapacity(env,jevtbuf);
    struct pxprpc_rtbridge_event_s *evt=(*env)->GetDirectBufferAddress(env,jevtbuf);
    evt->eventSize=len;
    pxprpc_rtbridge_push_invoc(c,evt);
}

jlong Java_pxprpc_pipe_NativeHelper_allocRuntimeBridge(JNIEnv *env){
    return pxprpc_rtbridge_alloc();
}

void Java_pxprpc_pipe_NativeHelper_freeRuntimeBridge(JNIEnv *env,jlong rtb){
    pxprpc_rtbridge_free((void *)rtb);
    return pxprpc_rtbridge_alloc();
}