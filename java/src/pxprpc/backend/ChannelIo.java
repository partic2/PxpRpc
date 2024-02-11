package pxprpc.backend;

import pxprpc.base.AbstractIo;
import pxprpc.base.Utils;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;

public class ChannelIo implements AbstractIo {
    public ByteChannel chan;
    public ChannelIo(ByteChannel chan){
        this.chan=chan;
    }
    public Object writeLock=new Object();

    @Override
    public void send(ByteBuffer[] buffs) throws IOException {
        synchronized (this.writeLock){
            int cnt=0;
            for(ByteBuffer b:buffs) {
                cnt += b.remaining();
            }
            ByteBuffer concatBuf=ByteBuffer.allocate(1400);
            concatBuf.order(ByteOrder.LITTLE_ENDIAN);
            concatBuf.putInt(cnt);
            for(int i=0;i<buffs.length;i++) {
                if(concatBuf.remaining()<buffs[i].remaining()){
                    Utils.flip(concatBuf);
                    Utils.writef(chan,concatBuf);
                    Utils.setLimit(concatBuf,concatBuf.capacity());
                    if(concatBuf.remaining()<buffs[i].remaining()){
                        Utils.writef(chan,buffs[i]);
                    }else{
                        concatBuf.put(buffs[i].duplicate());
                    }
                }else{
                    concatBuf.put(buffs[i].duplicate());
                }
            }
            Utils.flip(concatBuf);
            Utils.writef(chan,concatBuf);
        }
    }

    @Override
    public void receive(ByteBuffer[] buffs) throws IOException {
        int fixedSize=0;
        for(int i=0;i<buffs.length-1;i++){
            fixedSize+=buffs[i].remaining();
        }
        int remain=0;
        if(fixedSize<1400){
            //small packet, concat it
            ByteBuffer buff=ByteBuffer.allocate(fixedSize+4);
            buff.order(ByteOrder.LITTLE_ENDIAN);
            Utils.readf(chan,buff);
            remain=buff.getInt();
            for(int i=0;i<buffs.length-1;i++){
                remain-=buffs[i].remaining();
                buffs[i].duplicate().put(buff);
            }
        }else{
            remain=Utils.readInt32(chan);
            for(int i=0;i<buffs.length-1;i++){
                remain-=buffs[i].remaining();
                Utils.readf(chan,buffs[i]);
            }
        }
        ByteBuffer rbuf = ByteBuffer.allocate(remain);
        buffs[buffs.length-1]=rbuf;
        Utils.readf(chan,rbuf);
    }

    @Override
    public void close() {
        try {
            chan.close();
        } catch (IOException e) {
        }
    }
}
