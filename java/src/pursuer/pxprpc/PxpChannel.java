package pursuer.pxprpc;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;

public class PxpChannel implements ByteChannel {
    public ByteBuffer writeBuf;
    public ByteChannel chan;
    public int boundSession;
    public Object bufLock=new Object();
    public void setBuf(boolean enabled) throws IOException {
        synchronized (bufLock){
            if(this.writeBuf==null && enabled){
                this.writeBuf=ByteBuffer.allocate(1024);
            }else if(this.writeBuf!=null && !enabled){
                this.flush();
                this.writeBuf=null;
            }
        }
    }
    public PxpChannel wrap(ByteChannel chan){
        this.chan=chan;
        return this;
    }
    @Override
    public int read(ByteBuffer dst) throws IOException {
        return chan.read(dst);
    }

    @Override
    public int write(ByteBuffer src) throws IOException {
        synchronized (bufLock){
            if(writeBuf==null)return chan.write(src);
            int cnt=src.remaining();
            if(writeBuf.remaining()<cnt){
                flush();
            }
            if(cnt>writeBuf.remaining()){
                //large packet
                return chan.write(src);
            }else{
                writeBuf.put(src);
                return cnt;
            }
        }
    }

    public void flush() throws IOException {
        synchronized (this.bufLock){
            Utils.flip(writeBuf);
            Utils.writef(chan,writeBuf);
            Utils.setPos(writeBuf,0);
            Utils.setLimit(writeBuf,writeBuf.capacity());
        }
    }

    @Override
    public boolean isOpen() {
        return chan.isOpen();
    }

    @Override
    public void close() throws IOException {
        chan.close();
    }

}
