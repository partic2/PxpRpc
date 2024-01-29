package pxprpc.backend;

import pxprpc.base.Utils;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;

public class ChannelFromStream implements ByteChannel {
    public InputStream in;
    public OutputStream out;
    public void ChannelFromStream(InputStream in, OutputStream out){
        this.in=in;
        this.out=out;
    }
    @Override
    public int read(ByteBuffer dst) throws IOException {
        if(dst.hasArray()){
            int read=in.read(dst.array(), dst.position(), dst.remaining());
            Utils.setPos(dst,dst.position()+read);
            return read;
        }else{
            ByteBuffer rbuf = ByteBuffer.allocate(dst.remaining());
            int read=in.read(rbuf.array(), dst.position(), dst.remaining());
            Utils.setLimit(rbuf,read);
            dst.put(rbuf);
            return read;
        }
    }

    @Override
    public int write(ByteBuffer src) throws IOException {
        if(src.hasArray()){
            int remain=src.remaining();
            out.write(src.array(),src.position(),src.remaining());
            out.flush();
            return remain;
        }else{
            int remain=src.remaining();
            ByteBuffer wbuf=ByteBuffer.allocate(src.remaining());
            wbuf.put(src);
            out.write(wbuf.array());
            out.flush();
            return remain;
        }
    }
    protected boolean open=true;
    @Override
    public boolean isOpen() {
        return open;
    }

    @Override
    public void close() throws IOException {
        if(open){
            open=false;
            in.close();
            out.close();
        }
    }
}
