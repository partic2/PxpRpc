

#include <jni.h>
#include <stdint.h>
#include <pthread.h>
#include <pxprpc.h>

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


#define jbridgeEvent_id_pipe_accept 1
#define jbridgeEvent_id_pipe_connected 2
#define jbridgeEvent_id_pipe_readDone 3
#define jbridgeEvent_id_pipe_sendDone 4


typedef struct jbridgeEvent_s{
    int id;
    struct jbridgeEvent_s *next;
    union{
        struct{
            uint64_t serveId;
            uint64_t connId; 
        }accept;
        struct{
            uint64_t connId;
        }connected;
        struct{
            uint64_t connId;
            uint64_t base;
            uint32_t len;
        }readDone;
        struct{
            uint64_t connId;
            uint64_t dataId;
        }writeDone;
    } p;
} jbridgeEvent;

static jbridgeEvent *firstEvt=NULL;
static jbridgeEvent *lastEvt=NULL;
static jbridgeEvent *processingEvt=NULL;
pthread_mutex_t eventMutex;
pthread_cond_t eventSignal;


jint Java_pxprpc_pipe_NativeHelper_pointerSize(JNIEnv *env){
    return sizeof(void *);
}

jobject Java_pxprpc_pipe_NativeHelper_pullEvent(JNIEnv *env){
    if(processingEvt!=NULL){
        pxprpc__free(processingEvt);
    }
    pthread_mutex_lock(&eventMutex);
    while(firstEvt==NULL){
        pthread_cond_wait(&eventSignal,&eventMutex);
    }
    processingEvt=firstEvt;
    firstEvt=firstEvt->next;
    pthread_mutex_unlock(&eventMutex);
    return (*env)->NewDirectByteBuffer(env,processingEvt,sizeof(jbridgeEvent));
}

void pxprpc_pipe_jnibridge_send_event(jbridgeEvent *evt){
    pthread_mutex_lock(&eventMutex);
    if(firstEvt==NULL){
        firstEvt=evt;
    }else{
        jbridgeEvent *je=firstEvt;
        while(je->next!=NULL){
            je=je->next;
        }
        je->next=evt;
    }
    pthread_cond_signal(&eventSignal);
    pthread_mutex_unlock(&eventMutex);
}

void Java_pxprpc_pipe_NativeHelper_sendEvent(JNIEnv *env,jobject jbuf){
    int len=(*env)->GetDirectBufferCapacity(env,jbuf);
    void *base=(*env)->GetDirectBufferAddress(env,jbuf);
    jbridgeEvent *evt=pxprpc__malloc(len);
    memmove(evt,base,len);
    pxprpc_pipe_jnibridge_send_event(evt);
}

void Java_pxprpc_pipe_NativeHelper_pipeServe(JNIEnv *env,jobject name){
    /* TODO */
}

void Java_pxprpc_pipe_NativeHelper_pipeConnect(JNIEnv *env,jobject name){
    /* TODO */
}

void Java_pxprpc_pipe_NativeHelper_pipeSend(JNIEnv *env,jobject name){
    /* TODO */
}

void Java_pxprpc_pipe_NativeHelper_pipeReceive(JNIEnv *env,jobject name){
    /* TODO */
}

void Java_pxprpc_pipe_NativeHelper_pipeGetError(JNIEnv *env,jobject name){
    /* TODO */
}

void Java_pxprpc_pipe_NativeHelper_pipeFreeBuf(JNIEnv *env,jobject name){
    /* TODO */
}

void Java_pxprpc_pipe_NativeHelper_pipeClose(JNIEnv *env,jobject name){
    /* TODO */
}