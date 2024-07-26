package pxprpc.base;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/* Servcer side request */
public class PxpRequest {
    public ServerContext context;
    public int session;
    public int callableIndex;
    public ByteBuffer parameter;
    public ByteBuffer[] result=new ByteBuffer[0];
    protected AbstractIo chan;
    public Throwable rejected;
    public PxpRequest nextPending=null;
    public boolean inSequence=false;
    public void done() {
        try {
            this.chan = context.io2;
            if(this.rejected!=null){
                this.result=new ByteBuffer[]{ByteBuffer.wrap(this.rejected.toString().getBytes(ServerContext.charset))};
            }
            try {
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
            } finally {
                context.processRequest(context.finishRequest(this));
            }
        } catch (Exception ex) {
            /* mute all exception here. */
        }
    }
}
