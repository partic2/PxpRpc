package pxprpc.pipe;

import java.nio.ByteBuffer;

public class NativeHelper{
    public static native ByteBuffer accessMemory(long base, int length);
    public static native long directBufferProperty(ByteBuffer b,int fieldId);
    public static native int pointerSize();
    public static native ByteBuffer pullResult(long rtb);
    public static native void pushInvoc(long rtb,ByteBuffer invoc);
    public static native long allocRuntimeBridge();
    public static native long freeRuntimeBridge(long rtb);

}

