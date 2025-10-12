package pxprpc.runtimebridge;

import pxprpc.base.AbstractIo;
import pxprpc.base.ClientContext;
import pxprpc.base.RemoteError;
import pxprpc.extend.RpcExtendClient1;
import pxprpc.extend.RpcExtendClientCallable;
import pxprpc.extend.RpcExtendClientObject;
import xplatj.javaplat.partic2.util.OneArgRunnable;

import java.io.Closeable;
import java.io.IOException;

public class PipeServer implements Closeable {

    public String servName;
    public PipeServer(String servName){
        this.servName=servName;
    }
    RpcExtendClientObject rserv;
    public PipeServer serve()  {
        RuntimeBridgeUtils.ensureInit();
        try {
            rserv= (RpcExtendClientObject) RuntimeBridgeUtils.fServe.callBlock(this.servName)[0];
        } catch (IOException e) {
            throw new RuntimeException(e);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return this;
    }
    public static class IoA extends Io{
        RpcExtendClientObject conn;
        public void prepare(){
            try {
                nativeId= (long) RuntimeBridgeUtils.fIo2RawAddr.callBlock(this.conn)[0];
            } catch (IOException e) {
                throw new RuntimeException(e);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public void close() {
            this.conn.free();
        }
    }
    //Poll mode, can only call once.
    public void accept(final OneArgRunnable<AbstractIo> onNewConnection){
        try {
            RuntimeBridgeUtils.fAccept.poll(new RpcExtendClientCallable.Ret() {
                @Override
                public void cb(final Object[] r, final RemoteError err) {
                    if(err!=null){
                        return;
                    }
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            IoA io1 = new IoA();
                            io1.conn=(RpcExtendClientObject)r[0];
                            io1.prepare();
                            if(io1.nativeId==0){
                                return;
                            }
                            onNewConnection.run(io1);
                        }
                    }).start();
                }
            },rserv);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
    //once call mode
    public AbstractIo acceptBlock() {
        try {
            RpcExtendClientObject obj = (RpcExtendClientObject) RuntimeBridgeUtils.fAccept.callBlock(rserv)[0];
            IoA io1 = new IoA();
            io1.conn=obj;
            io1.prepare();
            if(io1.nativeId==0){
                return null;
            }
            return io1;
        } catch (IOException e) {
            throw new RuntimeException(e);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
    public void close(){
        if(rserv!=null){
            rserv.free();
        }
    }
}
