package pxprpc.backend;

import pxprpc.base.AbstractIo;
import pxprpc.base.Utils;

import java.io.EOFException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;
import java.nio.channels.GatheringByteChannel;
import java.nio.channels.ScatteringByteChannel;
import java.util.ArrayList;


public class ChannelIo2 implements AbstractIo {
    public GatheringByteChannel w;
    public ScatteringByteChannel r;
    public ChannelIo2(ScatteringByteChannel r,GatheringByteChannel w){
        this.r=r;
        this.w=w;
    }
    public Object writeLock=new Object();
    @Override
    public void send(ByteBuffer[] buffs) throws IOException {
        synchronized (this.writeLock){
            int cnt=0;
            for(ByteBuffer b:buffs){
                cnt+=b.remaining();
            }
            ByteBuffer cntBuf = ByteBuffer.allocate(4);
            cntBuf.order(ByteOrder.LITTLE_ENDIAN);
            cntBuf.putInt(cnt);
            Utils.setPos(cntBuf,0);
            ArrayList<ByteBuffer> writeBuff = new ArrayList<ByteBuffer>(buffs.length+1);
            writeBuff.add(cntBuf);
            for(int i=0;i<buffs.length;i++){
                /* write will block when get empty bytebuffer on android, so remove it */
                if(buffs[i].remaining()>0){
                    writeBuff.add(buffs[i]);
                }
            }
            ByteBuffer[] bbarr=writeBuff.toArray(new ByteBuffer[0]);
            while(bbarr[bbarr.length-1].remaining()>0){
                w.write(bbarr);
            }
        }
    }

    @Override
    public ByteBuffer receive() throws IOException {
        int length=Utils.readInt32(r);
        ByteBuffer buf=ByteBuffer.allocate(length);
        Utils.readf(r,buf);
        return buf;
    }

    @Override
    public void close() {
        try {
            w.close();
            r.close();
        } catch (IOException e) {
        }
    }
}
