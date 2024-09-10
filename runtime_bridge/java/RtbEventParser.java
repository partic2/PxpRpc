package pxprpc.pipe;

import pxprpc.base.Utils;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class RtbEventParser {
    protected ByteBuffer buf;
    public RtbEventParser use(ByteBuffer buf){
        this.buf=buf;
        this.buf.order(ByteOrder.LITTLE_ENDIAN);
        return this;
    }
    public static final short TYPE_ACCEPT=1;
    public static final short TYPE_CONNECTED=2;
    public static final short TYPE_RECVEIVEDONE=3;
    public static final short TYPE_SENDDONE=4;
    public static final short TYPE_SERVE=0x11;
    public static final short TYPE_CONNECT=0x12;
    public static final short TYPE_RECEIVE=0x13;
    public static final short TYPE_SEND=0x14;
    public static final short TYPE_CLOSE=0x15;
    public ByteBuffer eventSize(){return Utils.setPos(this.buf,0);}
    public ByteBuffer type(){
        return Utils.setPos(this.buf,4);
    }
    public ByteBuffer processed(){return Utils.setPos(this.buf,6);}
    public ByteBuffer connId(){return Utils.setPos(this.buf,24);}
    public ByteBuffer commonP(){return Utils.setPos(this.buf,32);}
    public ByteBuffer connectP(){return Utils.setPos(this.buf,24);}
    public ByteBuffer receiveDoneLen(){return Utils.setPos(this.buf,32);}
    public ByteBuffer serveEnable(){return Utils.setPos(this.buf,40);}
    public ByteBuffer sendDataSize(){return Utils.setPos(this.buf,40);}
    public ByteBuffer p3Data(){return Utils.setPos(this.buf,80);}
}
