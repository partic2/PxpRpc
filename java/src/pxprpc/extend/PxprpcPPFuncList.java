package pxprpc.extend;

import pxprpc.base.AbstractIo;

import java.io.IOException;
import java.nio.ByteBuffer;

public class PxprpcPPFuncList {
    public void io_send(AbstractIo io, ByteBuffer data) throws IOException {
        io.send(new ByteBuffer[]{data});
    }
    public ByteBuffer io_receive(AsyncReturn<ByteBuffer> ret,AbstractIo io) throws IOException {
        new Thread(new Runnable() {
            @Override
            public void run() {
                ByteBuffer[] r=new ByteBuffer[1];
                try {
                    io.receive(r);
                    ret.resolve(r[0]);
                } catch (IOException e) {
                    ret.reject(e);
                }
            }
        }).start();
        return null;
    }
}
