package pxprpc.base;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.ConcurrentHashMap;


public class ClientContext {
    public static interface Ret{
        void cb(ByteBuffer result, RemoteError exc);
    }
    public boolean running=false;
    protected ConcurrentHashMap<Integer,Ret> waitingSession=new ConcurrentHashMap<Integer,Ret>();

    protected AbstractIo io;
    public ClientContext backend1(AbstractIo io){
        this.io=io;
        return this;
    }

    public void run(){
        if(this.running)return;
        this.running=true;
        final ByteBuffer[] msgBody=new ByteBuffer[]{null};
        final ClientContext that=this;
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    while(that.running){
                        ByteBuffer allBuf = that.io.receive();
                        allBuf.order(ByteOrder.LITTLE_ENDIAN);
                        int sid = allBuf.getInt();
                        msgBody[0]=allBuf;
                        boolean isErr=(sid&0x80000000)!=0;
                        Ret r = that.waitingSession.get(sid&0x7fffffff);
                        if(r==null)
                            continue;
                        if(isErr){
                            byte[] errMsg=new byte[msgBody[0].remaining()];
                            msgBody[0].get(errMsg);
                            r.cb(null,new RemoteError(new String(errMsg,ServerContext.charset)));
                            that.waitingSession.remove(sid);
                        }else{
                            r.cb(msgBody[0],null);
                            that.waitingSession.remove(sid);
                        }
                    }
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        }).start();
    }

    public void call(int callableIndex,ByteBuffer parameter,int sid,Ret asyncResult) throws IOException {
        ByteBuffer header=ByteBuffer.allocate(8);
        header.order(ByteOrder.LITTLE_ENDIAN);
        header.putInt(sid).putInt(callableIndex);
        Utils.flip(header);
        this.waitingSession.put(sid,asyncResult);
        this.io.send(new ByteBuffer[]{header,parameter});
    }
    public ByteBuffer callBlock(int callableIndex,ByteBuffer parameter,int sid) throws IOException, InterruptedException {
        final Object lock=new Object();
        final ByteBuffer[] result=new ByteBuffer[1];
        final RemoteError[] exc=new RemoteError[1];
        Ret ar = new Ret() {
            @Override
            public void cb(ByteBuffer r, RemoteError e) {
                synchronized (lock){
                    result[0]=r;
                    exc[0]=e;
                    lock.notifyAll();
                }
            }
        };
        this.waitingSession.put(sid,ar);
        this.call(callableIndex,parameter,sid,ar);
        synchronized (lock){
            if(result[0]==null&&exc[0]==null){
                lock.wait();
            }
        }
        if(exc[0]!=null){
            throw exc[0];
        }
        return result[0];
    }
    //async version is not implemented because they are rarely used in Java. Maybe implemented with name like getFuncAsync?
    public int getFunc(String fnName,int sid) throws IOException, InterruptedException {
        return this.callBlock(-1,ByteBuffer.wrap(fnName.getBytes(ServerContext.charset)),sid).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }
    public void freeRef(int[] index,int sid) throws IOException, InterruptedException {
        ByteBuffer p = ByteBuffer.allocate(index.length * 4).order(ByteOrder.LITTLE_ENDIAN);
        for(int i=0;i<index.length;i++){
            p.putInt(index[i]);
        }
        Utils.flip(p);
        this.callBlock(-2,p,sid);
    }
    public void close(int sid) throws IOException {
        this.call(-3, ByteBuffer.allocate(0), sid, new Ret() {
            @Override
            public void cb(ByteBuffer result, RemoteError exc) {
            }
        });
        this.running=false;
    }
    public String getInfo(int sid) throws IOException, InterruptedException {
        ByteBuffer r = this.callBlock(-4, ByteBuffer.allocate(0), sid);
        byte[] servInfo=new byte[r.remaining()];
        r.get(servInfo);
        return new String(servInfo,ServerContext.charset);
    }

    public void sequence(int mask,int maskCnt,int sid) throws IOException, InterruptedException {
        ByteBuffer p = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
        p.putInt(maskCnt);
        Utils.flip(p);
        this.callBlock(-5,p,sid);
    }

}
