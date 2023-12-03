package pursuer.pxprpc;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;
import java.util.concurrent.locks.Lock;

public class PxpRequest implements AsyncReturn<Object> {
    public ServerContext context;
    public int session;
    public int opcode;
    public int destAddr;
    public int srcAddr;
    public Object parameter;
    public Object result;
    public PxpCallable callable;
    public boolean pending = false;
    protected PxpChannel chan;
    @Override
    public void result(Object result) {
        this.result = result;
        context.putRefSlots(destAddr, new PxpObject(result));
        pending = false;
        try {
            this.chan = context.responseStart(session);
            try {
                callable.writeResult(this);
            } catch (IOException ex) {
            } finally {
                context.responseStop(chan);
            }
        } catch (Exception ex) {
        }
    }

    public PxpChannel getChan() {
        if (chan == null) {
            return context.chan;
        }
        return chan;
    }
}
