package pxprpc.backend;

import pxprpc.base.AbstractIo;
import pxprpc.base.Utils;

import java.io.IOException;
import java.nio.ByteBuffer;
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
            for(ByteBuffer b:buffs){
                cnt+=b.remaining();
            }
            Utils.writeInt32(chan,cnt);
            for(ByteBuffer b:buffs){
                Utils.writef(chan,b);
            }
        }
    }

    @Override
    public void receive(ByteBuffer[] buffs) throws IOException {
        int remain=Utils.readInt32(chan);
        for(int i=0;i<buffs.length-1;i++){
            remain-=buffs[i].remaining();
            Utils.readf(chan,buffs[i]);
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
