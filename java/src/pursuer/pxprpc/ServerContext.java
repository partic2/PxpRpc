package pursuer.pxprpc;

import java.io.Closeable;
import java.io.IOException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class ServerContext implements Closeable {
    public static int DefaultRefSlotsCap = 256;
    protected PxpChannel chan;
    public boolean running = true;
    public PxpObject[] refSlots = new PxpObject[DefaultRefSlotsCap];
    public Map<String, Object> funcMap = new HashMap<String, Object>();
    protected BuiltInFuncList builtIn;
    public static final Charset charset=Charset.forName("utf-8");
    public void init(ByteChannel chan) {
        this.chan = new PxpChannel().wrap(chan);
        builtIn = new BuiltInFuncList();
        funcMap.put("builtin", builtIn);
    }
    public int bufferedSession=0xffffffff;
    public int bufferedMaskBitsCnt=0;
    public void serve() throws IOException {
        while (running) {
            PxpRequest r = new PxpRequest();
            r.context = this;
            byte[] session = new byte[4];
            Utils.readf(chan, ByteBuffer.wrap(session));
            r.session = ByteBuffer.wrap(session).order(ByteOrder.LITTLE_ENDIAN).getInt();
            r.opcode = session[0];
            switch (r.opcode) {
                case 1:
                    r.destAddr = Utils.readInt32(chan);
                    int len = Utils.readInt32(chan);
                    byte[] buf=new byte[len];
                    Utils.readf(chan, ByteBuffer.wrap(buf));
                    r.parameter = buf;
                    push(r);
                    break;
                case 2:
                    r.srcAddr = Utils.readInt32(chan);
                    pull(r);
                    break;
                case 3:
                    r.destAddr = Utils.readInt32(chan);
                    r.srcAddr = Utils.readInt32(chan);
                    assign(r);
                    break;
                case 4:
                    r.destAddr = Utils.readInt32(chan);
                    unlink(r);
                    break;
                case 5:
                    r.destAddr = Utils.readInt32(chan);
                    r.srcAddr = Utils.readInt32(chan);
                    r.callable = (PxpCallable) refSlots[r.srcAddr].get();
                    r.callable.readParameter(r);
                    call(r);
                    break;
                case 6:
                    r.destAddr = Utils.readInt32(chan);
                    r.srcAddr = Utils.readInt32(chan);
                    getFunc(r);
                    break;
                case 7:
                    close();
                    running = false;
                    break;
                case 8:
                    getInfo(r);
                    break;
                case 9:
                    r.parameter=Utils.readInt32(chan);
                    buffer(r);
                    break;

            }
        }
        running = false;
    }

    public void putRefSlots(int addr, PxpObject r) {
        if (refSlots[addr] != null)
            refSlots[addr].release();
        if (r != null)
            r.addRef();
        refSlots[addr] = r;
    }

    public void push(final PxpRequest r) throws IOException {
        putRefSlots(r.destAddr, new PxpObject(r.parameter));
        PxpChannel chan=responseStart(r.session);
        responseStop(chan);
    }

    public void pull(final PxpRequest r) throws IOException {
        Object o = null;
        if (refSlots[r.srcAddr] != null) {
            o = refSlots[r.srcAddr].get();
        }
        PxpChannel chan=responseStart(r.session);
        try {
            if (o instanceof ByteBuffer) {
                Utils.writeInt32(chan, ((Buffer) o).remaining());
                Utils.writef(chan, (ByteBuffer) o);
            } else if (o instanceof byte[]) {
                byte[] b = (byte[]) o;
                Utils.writeInt32(chan, b.length);
                Utils.writef(chan, ByteBuffer.wrap(b));
            } else if (o instanceof String) {
                byte[] b = ((String) o).getBytes("utf-8");
                Utils.writeInt32(chan, b.length);
                Utils.writef(chan, ByteBuffer.wrap(b));
            } else {
                Utils.writeInt32(chan, -1);
            }
        }finally{
            responseStop(chan);
        }
    }

    public void assign(final PxpRequest r) throws IOException {
        putRefSlots(r.destAddr, this.refSlots[r.srcAddr]);
        PxpChannel chan=responseStart(r.session);
        responseStop(chan);
    }

    public void unlink(final PxpRequest r) throws IOException {
        putRefSlots(r.destAddr, null);
        PxpChannel chan=responseStart(r.session);
        responseStop(chan);
    }

    public void call(final PxpRequest r) throws IOException {
        r.pending = true;
        r.callable.call(r, r);
    }

    public void getFunc(final PxpRequest r) throws IOException {
        String name = getStringAt(r.srcAddr);
        int namespaceDelim = name.lastIndexOf(".");
        String namespace = name.substring(0, namespaceDelim);
        String func = name.substring(namespaceDelim + 1);
        Object obj = funcMap.get(namespace);
        PxpCallable found = null;
        if (obj != null) {
            found = builtIn.getBoundMethod(obj, func);
        }
        PxpChannel chan=responseStart(r.session);
        try{
            if (found == null) {
                Utils.writeInt32(chan,0);
            } else {
                putRefSlots(r.destAddr, new PxpObject(found));
                Utils.writeInt32(chan,r.destAddr);
            }
        }finally{
            responseStop(chan);
        }
    }

    public void getInfo(final PxpRequest r) throws IOException {
        PxpChannel chan=responseStart(r.session);
        try{
            byte[] b = (
                    "server name:pxprpc for java\n" +
                            "version:1.1\n" +
                            "reference slots capacity:" + this.refSlots.length + "\n"
            ).getBytes("utf-8");
            Utils.writeInt32(chan,b.length);
            Utils.writef(chan,ByteBuffer.wrap(b));
        }finally {
            responseStop(chan);
        }
    }
    public void buffer(final PxpRequest r) throws IOException {
        bufferedSession=(int)r.parameter;
        if(bufferedSession==0xffffffff){
            //no buffer, default value.
            chan.setBuf(false);
        }else{
            chan.setBuf(true);
            bufferedMaskBitsCnt=bufferedSession&0xff;
            bufferedSession=bufferedSession>>(32-bufferedMaskBitsCnt);
        }
    }

    public PxpChannel responseStart(int session) throws IOException {
        writeLock().lock();
        chan.boundSession=session;
        Utils.writeInt32(chan,session);
        return chan;
    }
    public void responseStop(PxpChannel chan) throws IOException {
        if(bufferedSession!=0xffffffff){
            if((chan.boundSession>>(32-bufferedMaskBitsCnt))!=bufferedSession){
                chan.flush();
            }else{
                System.currentTimeMillis();
            }
        }
        writeLock().unlock();
    }

    private ReentrantLock writeLock = new ReentrantLock();

    public Lock writeLock() {
        return writeLock;
    }
    public byte[] getBytesAt(int addr) {
        Object b = refSlots[addr].get();
        if(b instanceof ByteBuffer) {
            ByteBuffer b2=(ByteBuffer) b;
            Buffer b3=b2;
            b3.mark();
            byte[] ba=new byte[b2.remaining()];
            b2.get(ba);
            b3.reset();
            return ba;
        }else {
            return (byte[])b;
        }
    }
    public String getStringAt(int addr) {
        if (refSlots[addr] == null) {
            return "";
        }
        Object o = refSlots[addr].get();
        if (o instanceof ByteBuffer || o instanceof byte[]) {
            return new String(getBytesAt(addr));
        } else {
            return o.toString();
        }
    }

    private void closeQuietly(Closeable c) {
        try {
            c.close();
        } catch (Exception e) {
        }
        ;
    }

    @Override
    public void close() throws IOException {
        running = false;
        closeQuietly(chan);
        for (int i1 = 0; i1 < refSlots.length; i1++) {
            if (refSlots[i1] != null) {
                refSlots[i1].release();
            }
        }
        System.gc();
    }
}
