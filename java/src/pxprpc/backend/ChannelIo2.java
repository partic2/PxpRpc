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

// For better performance than ChannelIo
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
            ByteBuffer[] writeBuff = new ByteBuffer[buffs.length + 1];
            writeBuff[0]=cntBuf;
            for(int i=1;i<writeBuff.length;i++){
                writeBuff[i]=buffs[i-1].duplicate();
            }
            w.write(writeBuff);
        }
    }

    @Override
    public void receive(ByteBuffer[] buffs) throws IOException {
        ByteBuffer[] readBuff=new ByteBuffer[buffs.length];
        readBuff[0]=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
        for(int i=1;i<readBuff.length;i++){
            readBuff[i]=buffs[i-1].duplicate();
        }
        while(true){
            if(r.read(readBuff)<0){
                throw new EOFException();
            };
            if(readBuff[readBuff.length-1].remaining()==0){
                break;
            }
        }
        Utils.setPos(readBuff[0],0);
        int remain=readBuff[0].getInt();
        for(int i=0;i<buffs.length-1;i++){
            remain-=buffs[i].remaining();
        }
        ByteBuffer rbuf = ByteBuffer.allocate(remain);
        buffs[buffs.length-1]=rbuf.duplicate();
        if(remain>0){
            while(true){
                if(r.read(rbuf)<0){
                    throw new EOFException();
                };
                if(rbuf.remaining()==0){
                    break;
                }
            }
        }
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
