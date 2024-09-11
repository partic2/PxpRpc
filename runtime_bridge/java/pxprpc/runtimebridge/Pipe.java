package pxprpc.runtimebridge;

import pxprpc.base.Utils;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;

public class Pipe {
    public static Charset charset=Charset.forName("utf-8");
    public static Io connect(String s){
        byte[] b = s.getBytes(charset);
        ByteBuffer b2 = ByteBuffer.allocateDirect(b.length+1);
        b2.put(b);
        b2.put((byte)0);
        Utils.setPos(b2,0);
        long nativeId=NativeHelper.pipeConnect(b2);
        if(nativeId==0){
            return null;
        }else{
            Io io = new Io();
            io.nativeId=nativeId;
            return io;
        }
    }
}
