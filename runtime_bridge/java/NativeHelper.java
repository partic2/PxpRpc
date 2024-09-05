package pxprpc.runtimebridge;

import java.nio.ByteBuffer;

public class NativeHelper{
    public static native ByteBuffer accessMemory(long base, int length);
    public static native long directBufferProperty(ByteBuffer b,int fieldId);
    public static native int pointerSize();
    //"servName" MUST end with '\0'
    public static native long pipeConnect(ByteBuffer servName);
    public static native void ioSend(long io,ByteBuffer nativeBuffer,ByteBuffer errorString);
    public static native ByteBuffer ioReceive(long io,ByteBuffer errorString);
    public static native void ioBufFree(long io,ByteBuffer b);
    public static native void ioClose(long io);
    public static native void ensureRtbInited(ByteBuffer errorString);
}

