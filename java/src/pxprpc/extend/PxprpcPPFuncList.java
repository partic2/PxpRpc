package pxprpc.extend;

import pxprpc.base.*;

import java.io.IOException;
import java.nio.ByteBuffer;

public class PxprpcPPFuncList {
    public void io_send(AbstractIo io, ByteBuffer data) throws IOException {
        io.send(new ByteBuffer[]{data});
    }
    public ByteBuffer io_receive(final AsyncReturn<ByteBuffer> ret,final AbstractIo io) throws IOException {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    ret.resolve(io.receive());
                } catch (IOException e) {
                    ret.reject(e);
                }
            }
        }).start();
        return null;
    }

    public PxpCallable io_set_auto_close=new PxpCallable() {
        @Override
        public void call(PxpRequest req) {
            Serializer2 ser = new Serializer2().prepareUnserializing(req.parameter);
            int index = ser.getInt();
            PxpRef ref = req.context.getRef(index);
            ref.onFree=null;
        }
    };
}
