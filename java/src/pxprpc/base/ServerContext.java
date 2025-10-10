package pxprpc.base;


import java.io.Closeable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;

public class ServerContext implements Closeable {
    public static int RefPoolExpandCount = 256;
    protected AbstractIo io2;
    public boolean running = true;
    public PxpRef[] refPool = new PxpRef[0];
    public PxpRef freeRefEntry =null;
    public Object refPoolLock=new Object();
    public FuncMap funcMap;
    public static final Charset charset=Charset.forName("utf-8");
    public ConcurrentLinkedQueue eventQueue=new ConcurrentLinkedQueue();
    public void expandRefPools(){
        synchronized (refPoolLock){
            int start=this.refPool.length;
            int end=this.refPool.length+RefPoolExpandCount-1;
            this.refPool=Arrays.copyOf(this.refPool,end+1);
            for(int i=start;i<=end;i++){
                refPool[i]=new PxpRef(i);
            }
            for(int i=start;i<end;i++){
                refPool[i].nextFree=refPool[i+1];
            }
            refPool[end].nextFree=freeRefEntry;
            freeRefEntry=refPool[start];
        }
    }
    public PxpRef allocRef(){
        synchronized (refPoolLock){
            if(freeRefEntry==null){
                this.expandRefPools();
            }
            PxpRef ref2 = freeRefEntry;
            freeRefEntry = freeRefEntry.nextFree;
            return ref2;
        }
    }
    public void freeRef(PxpRef ref2){
        synchronized (refPoolLock){
            ref2.free();
            ref2.nextFree=freeRefEntry;
            freeRefEntry=ref2;
        }
    }
    public PxpRef getRef(int index){
        return this.refPool[index];
    }
    public void init(AbstractIo chan,FuncMap fnmap) {
        this.io2 = chan;
        this.expandRefPools();
        if(fnmap==null){
            try {
                Class<?> cls = Class.forName("pxprpc.extend.DefaultFuncMap");
                this.funcMap=(FuncMap) cls.getMethod("getDefault").invoke(null);
            } catch (Exception e) {
            }
        }else{
            this.funcMap=fnmap;
        }
    }
    public void serve() throws IOException {
        while (running) {
            PxpRequest r = new PxpRequest();
            r.context = this;
            ByteBuffer buf = this.io2.receive();
            buf.order(ByteOrder.LITTLE_ENDIAN);
            r.session=buf.getInt();
            r.callableIndex=buf.getInt();
            r.parameter=buf;
            processRequest(r);
        }
        running = false;
    }

    public void processRequest(PxpRequest r) throws IOException {
        if(r==null)return;
        if (r.callableIndex >= 0) {
            r.callNonbuiltinCallable();
        } else {
            //pxprpc basic function
            switch (-r.callableIndex) {
                case 1:
                    getFunc(r);
                    break;
                case 2:
                    freeRefHandler(r);
                    break;
                case 3:
                    close();
                    running = false;
                    break;
                case 4:
                    getInfo(r);
                    break;
                case 5:
                    poll(r);
                    break;
                default:
                    throw new RuntimeException("No matched function");
            }
        }
    }

    public void getFunc(final PxpRequest r){
        String name = new String(Utils.toBytes(r.parameter),charset);
        PxpCallable found = funcMap.get(name);
        Serializer2 ser=new Serializer2().prepareSerializing(4);
        if(found!=null){
            PxpRef ref2=allocRef();
            ref2.set(found,null);
            ser.putInt(ref2.index);
        }else{
            ser.putInt(-1);
        }
        r.result=new ByteBuffer[]{ser.build()};
        r.done();
    }
    public void freeRefHandler(final PxpRequest r){
        int count=r.parameter.remaining()>>>2;
        r.parameter.order(ByteOrder.LITTLE_ENDIAN);
        for(int i=0;i<count;i++) {
            freeRef(getRef(r.parameter.getInt()));
        }
        r.done();
    }

    public void getInfo(final PxpRequest r) throws IOException {
        byte[] b = (
                "server name:pxprpc for java\n" +
                        "version:2.0\n"
        ).getBytes(charset);
        r.result=new ByteBuffer[]{ByteBuffer.wrap(b)};
        r.done();
    }

    public void poll(final PxpRequest r) throws IOException{
        r.parameter.order(ByteOrder.LITTLE_ENDIAN);
        r.callableIndex=r.parameter.getInt();
        r.pollCall=true;
        r.callNonbuiltinCallable();
    }


    @Override
    public void close() throws IOException {
        //Thread safe?
        if(!running)return;
        running = false;
        io2.close();
        for (int i1 = 0; i1 < refPool.length; i1++) {
            PxpRef ref2 = refPool[i1];
            if(ref2.object1 !=null){
                ref2.free();
            }
        }
        System.gc();

    }
}
