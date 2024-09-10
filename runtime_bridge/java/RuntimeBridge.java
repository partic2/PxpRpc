package pxprpc.pipe;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class RuntimeBridge {
    public long rtb;

    public RuntimeBridge(long rtbId) {
        this.rtb = rtbId;
    }

    public RuntimeBridge() {
        this.rtb = NativeHelper.allocRuntimeBridge();
    }

    //HashMap<Long,NativePipeIo|NativePipeServer>
    protected HashMap<Long, Object> refs = new HashMap<Long, Object>();
    boolean running = true;

    //Block current thread for event loop
    public void startServing() {
        while (running) {
            ByteBuffer evt = NativeHelper.pullResult(rtb);
            RtbEventParser parser = new RtbEventParser().use(evt);
            short type = parser.type().getShort();
            switch (type) {
                case RtbEventParser.TYPE_ACCEPT: {
                    long connId = parser.connId().getLong();
                    NativePipeIo conn = new NativePipeIo();
                    conn.connId = connId;
                    conn.rtb = this;
                    refs.put(connId, conn);
                    long servId = parser.commonP().getLong();
                    NativePipeServer serv = (NativePipeServer) refs.get(servId);
                    serv.queuedConnection.offer(conn);
                    if (serv.onConnected != null) {
                        serv.onConnected.run();
                    }
                    parser.processed().put((byte) 1);
                }
                break;
                case RtbEventParser.TYPE_CONNECTED: {
                    long connId = parser.connId().getLong();
                    NativePipeIo conn = new NativePipeIo();
                    conn.connId = connId;
                    conn.rtb = this;
                    int p = parser.commonP().getInt();
                    refs.put(connId, conn);
                    waittingConnectedLock.lock();
                    try {
                        waittingConnected.get(p).notifyAll();
                        waittingConnected.set(p, null);
                        for (int i = waittingConnected.size(); i >= 0; i--) {
                            if (waittingConnected.get(i) == null) {
                                waittingConnected.remove(i);
                            }
                        }
                    } finally {
                        waittingConnectedLock.unlock();
                    }
                    parser.processed().put((byte) 1);
                }
                break;
                case RtbEventParser.TYPE_RECVEIVEDONE: {
                    long connId = parser.connId().getLong();
                    NativePipeIo conn = (NativePipeIo) refs.get(connId);
                    int recvlen = parser.receiveDoneLen().getInt();
                    ByteBuffer recvbuf = ByteBuffer.allocate(recvlen);
                    //we copy the buffer, not so efficient but avoid buffer leak.
                    recvbuf.put(parser.p3Data());
                    try {
                        conn.recvBuffers.put(recvbuf);
                    } catch (InterruptedException e) {
                    }
                    parser.processed().put((byte) 1);
                }
                break;
                case RtbEventParser.TYPE_SENDDONE: {
                    //do nothing?
                }
                break;

            }
        }
    }

    protected ArrayList<NativePipeIo> waittingConnected = new ArrayList<NativePipeIo>();
    protected ReentrantLock waittingConnectedLock = new ReentrantLock();

    public NativePipeIo connect(String serverName) {
        waittingConnectedLock.lock();
        try {
            int p = this.waittingConnected.size();
            Condition cond = waittingConnectedLock.newCondition();
            NativePipeIo pipe = new NativePipeIo();
            pipe.connectedSignal = cond;
            this.waittingConnected.add(pipe);
            ByteBuffer invoc = ByteBuffer.allocateDirect(88);
            RtbEventParser parser = new RtbEventParser().use(invoc);
            parser.processed().put((byte) 0);
            parser.connectP().putInt(p);
            parser.type().putShort(RtbEventParser.TYPE_CONNECT);
            NativeHelper.pushInvoc(rtb, invoc);
            cond.wait();
            if (pipe.connId != 0) {
                return pipe;
            } else {
                return null;
            }
        } catch (InterruptedException e) {
            return null;
        } finally {
            waittingConnectedLock.unlock();
        }
    }

    public LinkedList<ByteBuffer> pinedBuffer = new LinkedList<ByteBuffer>();

    public void postSendInvoc(long connId, ByteBuffer[] data) {
        Iterator<ByteBuffer> it = pinedBuffer.iterator();
        while (it.hasNext()) {
            ByteBuffer b = it.next();
            RtbEventParser parser = new RtbEventParser().use(b);
            if (parser.processed().get() != (byte) 0) {
                it.remove();
            }
        }
        int sendSize = 0;
        for (ByteBuffer d : data) {
            sendSize += d.remaining();
        }
        ByteBuffer invoc = ByteBuffer.allocateDirect(sendSize + 88);
        RtbEventParser parser = new RtbEventParser().use(invoc);
        parser.type().putShort(RtbEventParser.TYPE_SEND);
        parser.processed().put((byte) 0);
        parser.connId().putLong(connId);
        parser.sendDataSize().putInt(sendSize);
        ByteBuffer p3Data = parser.p3Data();
        for (ByteBuffer d : data) {
            p3Data.put(d);
        }
        pinedBuffer.push(invoc);
        NativeHelper.pushInvoc(this.rtb, invoc);
    }


    public ArrayList<NativePipeServer> runningServer = new ArrayList<NativePipeServer>();

    public NativePipeServer serve(String servName,boolean enable) {
        ByteBuffer servNameBuf;
        try {
            servNameBuf = ByteBuffer.wrap(servName.getBytes("utf-8"));
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        };
        ByteBuffer invoc = ByteBuffer.allocateDirect(88+servNameBuf.remaining());
        RtbEventParser parser = new RtbEventParser().use(invoc);
        parser.p3Data().put(servNameBuf);
        if(enable){
            int servId= runningServer.size();
            parser.commonP().putLong(servId);
            parser.serveEnable().put((byte)1);
            NativePipeServer serv = new NativePipeServer();
            serv.serveInvoc=invoc;
            serv.serverName=servName;
            runningServer.add(serv);
            return serv;
        }else{
            parser.serveEnable().put((byte)0);
            for (int i = runningServer.size(); i >= 0; i--) {
                if(runningServer.get(i)!=null && runningServer.get(i).serverName.equals(servName)){
                    this.runningServer.set(i,null);
                }
            }
            for (int i = runningServer.size(); i >= 0; i--) {
                if (runningServer.get(i) == null) {
                    runningServer.remove(i);
                }
            }
            pinedBuffer.add(invoc);
            return null;
        }
    }
}

