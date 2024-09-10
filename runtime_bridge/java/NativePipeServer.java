package pxprpc.pipe;

import pxprpc.base.AbstractIo;

import java.nio.ByteBuffer;
import java.util.concurrent.ConcurrentLinkedQueue;

public class NativePipeServer {
    public ByteBuffer serveInvoc;
    public Runnable onConnected;
    public String serverName;
    public ConcurrentLinkedQueue<AbstractIo> queuedConnection=new ConcurrentLinkedQueue<AbstractIo>();

}
