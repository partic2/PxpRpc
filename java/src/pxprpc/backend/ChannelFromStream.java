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
        //we expect no native buffer in parameter
        int read=in.read(dst.array(), dst.position(), dst.remaining());
        Utils.setPos(dst,dst.position()+read);
        return read;
    }

    @Override
    public int write(ByteBuffer src) throws IOException {
        int remain=src.remaining();
        out.write(src.array(),src.position(),src.remaining());
        out.flush();
        return remain;
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
