package pxprpc.runtimebridge;

import pxprpc.base.AbstractIo;
import pxprpc.base.Utils;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.annotation.Native;
import java.nio.ByteBuffer;

public class Io implements AbstractIo {
    public long nativeId;

    public static String readByteBufferString(ByteBuffer b){
        b.mark();
        byte[] r = new byte[b.remaining()];
        b.get(r);
        b.reset();
        try {
            return new String(r,"utf-8");
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
    }
    public static void clearErrorString(ByteBuffer b){
        Utils.setPos(b,0);
        b.put((byte)0);
        Utils.setPos(b,0);
    }
    public static void checkErrorString(ByteBuffer b) throws IOException{
        Utils.setPos(b,0);
        if(b.get()!=0){
            Utils.setPos(b,0);
            throw new IOException(readByteBufferString(b));
        }
    }
    public ByteBuffer sendError=ByteBuffer.allocateDirect(256);
    public ByteBuffer receiveError=ByteBuffer.allocateDirect(256);
    @Override
    public void send(ByteBuffer[] buffs) throws IOException {
        synchronized (sendError){
            int size=0;
            for(int i=0;i<buffs.length;i++){
                size+=buffs[i].remaining();
            }
            ByteBuffer b = ByteBuffer.allocateDirect(size);
            for(int i=0;i<buffs.length;i++) {
                b.put(buffs[i]);
            }
            clearErrorString(sendError);
            NativeHelper.ioSend(nativeId,b,sendError);
            checkErrorString(sendError);
        }
    }

    @Override
    public ByteBuffer receive() throws IOException {
        synchronized (receiveError){
            clearErrorString(receiveError);
            ByteBuffer recv=NativeHelper.ioReceive(nativeId,receiveError);
            checkErrorString(receiveError);
            ByteBuffer copy=ByteBuffer.allocate(recv.remaining());
            copy.put(recv);
            Utils.setPos(copy,0);
            NativeHelper.ioBufFree(nativeId,recv);
            return copy;
        }
    }

    @Override
    public void close() {
        NativeHelper.ioClose(nativeId);
    }
}
