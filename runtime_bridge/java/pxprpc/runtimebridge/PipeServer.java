package pxprpc.runtimebridge;

import pxprpc.base.AbstractIo;
import pxprpc.base.ClientContext;
import pxprpc.extend.RpcExtendClient1;
import pxprpc.extend.RpcExtendClientCallable;
import pxprpc.extend.RpcExtendClientObject;

import java.io.IOException;

public class PipeServer {
    public static RpcExtendClient1 client;
    public static RpcExtendClientCallable fServe;
    public static RpcExtendClientCallable fAccept;
    public static RpcExtendClientCallable fIo2RawAddr;
    public static void ensureInit(){
        if(client==null){
            Io io = Pipe.connect("/pxprpc/runtime_bridge/0");
            ClientContext client1 = new ClientContext();
            client1.backend1(io);
            client=new RpcExtendClient1(client1);
            try {
                client.init();
                fServe = client.getFunc("pxprpc_pipe_pp.serve").typedecl("s->o");
                fAccept = client.getFunc("pxprpc_pipe_pp.accept").typedecl("o->o");
                fIo2RawAddr=client.getFunc("pxprpc_pp.io_to_raw_addr").typedecl("o->l");
            } catch (IOException e) {
                throw new RuntimeException(e);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }
    public String servName;
    public PipeServer(String servName){
        this.servName=servName;
    }
    RpcExtendClientObject rserv;
    public PipeServer serve()  {
        ensureInit();
        try {
            rserv= (RpcExtendClientObject) fServe.callBlock(this.servName)[0];
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
                nativeId= (long) fIo2RawAddr.callBlock(this.conn)[0];
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
    public AbstractIo acceptBlock() {
        ensureInit();
        try {
            RpcExtendClientObject obj = (RpcExtendClientObject) fAccept.callBlock(rserv)[0];
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
    public void stop(){
        ensureInit();
        rserv.free();
    }
}
