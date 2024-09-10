package pxprpc.pipe;

import pxprpc.base.AbstractIo;
import pxprpc.base.Utils;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.locks.Condition;


public class NativePipeIo implements AbstractIo {
    public long connId;
    public Condition connectedSignal;
    public BlockingQueue<ByteBuffer> recvBuffers=new LinkedBlockingQueue<ByteBuffer>();
    public RuntimeBridge rtb;
    @Override
    public void send(ByteBuffer[] buffs) throws IOException {
        rtb.postSendInvoc(this.connId,buffs);
    }

    @Override
    public void receive(ByteBuffer[] buffs) throws IOException {
        try {
            ByteBuffer buf = recvBuffers.take();
            for(int i=0;i<buffs.length-1;i++){
                int savedLimit = buf.limit();
                Utils.setLimit(buf,buf.position()+buffs[i].remaining());
                buffs[i].put(buf);
                Utils.setLimit(buf,savedLimit);
            }
            buffs[buffs.length-1]=buf;
        } catch (InterruptedException e) {
            throw new IOException(e);
        }
    }

    @Override
    public void close() {
    }
}
