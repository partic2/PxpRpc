package pxprpc.base;

import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Serializer2 {
    public ByteBuffer buf;
    public Serializer2 prepareUnserializing(ByteBuffer buf){
        this.buf=buf;
        this.buf.order(ByteOrder.LITTLE_ENDIAN);
        return this;
    }
    public Serializer2 prepareSerializing(int initBufSize){
        this.buf=ByteBuffer.allocate(initBufSize);
        this.buf.order(ByteOrder.LITTLE_ENDIAN);
        return this;
    }
    public Serializer2 putInt(int val){
        buf=Utils.ensureBuffer(buf,4);
        buf.putInt(val);
        return this;
    }
    public Serializer2 putLong(long val){
        buf=Utils.ensureBuffer(buf,8);
        buf.putLong(val);
        return this;
    }
    public Serializer2 putFloat(float val){
        buf=Utils.ensureBuffer(buf,4);
        buf.putFloat(val);
        return this;
    }
    public Serializer2 putDouble(double val){
        buf=Utils.ensureBuffer(buf,8);
        buf.putDouble(val);
        return this;
    }
    public Serializer2 putVarint(int val){
        if(val>=0xff){
            buf=Utils.ensureBuffer(buf,5);
            buf.put((byte)0xff);
            buf.putInt(val);
        }else{
            buf=Utils.ensureBuffer(buf,1);
            buf.put((byte)val);
        }
        return this;
    }
    public Serializer2 putBytes(ByteBuffer b){
        putVarint(b.remaining());
        buf=Utils.ensureBuffer(buf,b.remaining());
        buf.put(b);
        return this;
    }
    public Serializer2 putString(String val){
        byte[] b=val.getBytes(ServerContext.charset);
        putBytes(ByteBuffer.wrap(b));
        return this;
    }
    public ByteBuffer build(){
        Buffer b=buf;
        b.flip();
        return buf;
    }
    public int getInt(){
        return buf.getInt();
    }
    public long getLong(){
        return buf.getLong();
    }
    public float getFloat(){
        return buf.getFloat();
    }
    public double getDouble(){
        return buf.getDouble();
    }
    public int getVarint(){
        int val=buf.get()&0xff;
        if(val==255){
            val=buf.getInt();
        }
        return val;
    }
    public ByteBuffer getBytes(){
        int len=getVarint();
        ByteBuffer r = buf.duplicate();
        Utils.setLimit(r,r.position()+len);
        Utils.setPos(buf,buf.position()+len);
        return r;
    }
    public String getString(){
        return new String(Utils.toBytes(getBytes()),ServerContext.charset);
    }

}
