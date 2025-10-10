package pxprpc.base;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/* Servcer side request */
public class PxpRequest {
    public ServerContext context;
    public int session;
    public int callableIndex;
    public ByteBuffer parameter;
    public ByteBuffer[] result=new ByteBuffer[0];
    public Throwable rejected;

    public void done() {
        try {
            this.chan = context.io2;
            if(this.rejected!=null){
                this.result=new ByteBuffer[]{ByteBuffer.wrap(this.rejected.toString().getBytes(ServerContext.charset))};
            }
            ByteBuffer[] sendBuf = new ByteBuffer[this.result.length + 1];
            sendBuf[0]=ByteBuffer.allocate(4);
            sendBuf[0].order(ByteOrder.LITTLE_ENDIAN);
            if(rejected==null){
                sendBuf[0].putInt(session);
            }else{
                sendBuf[0].putInt(session^0x80000000);
            }
            for(int i=1;i<sendBuf.length;i++){
                sendBuf[i]=this.result[i-1];
            }
            Utils.flip(sendBuf[0]);
            chan.send(sendBuf);
            if(this.pollCall && this.rejected==null && context.running){
                callNonbuiltinCallable();
            }
        } catch (Exception ex) {
            /* mute all exception here. */
        }
    }
    //pxprpc internal use.
    public boolean pollCall=false;
    protected AbstractIo chan;
    public void callNonbuiltinCallable() {
        this.rejected=null;
        this.result=null;
        PxpCallable callable = (PxpCallable) context.refPool[callableIndex].get();
        callable.call(this);
    }
}
