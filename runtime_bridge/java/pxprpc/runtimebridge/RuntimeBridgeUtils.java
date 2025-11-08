package pxprpc.runtimebridge;

import pxprpc.base.AbstractIo;
import pxprpc.base.ClientContext;
import pxprpc.base.ServerContext;
import pxprpc.extend.DefaultFuncMap;
import pxprpc.extend.RpcExtendClient1;
import pxprpc.extend.RpcExtendClientCallable;
import pxprpc.extend.RpcExtendClientObject;
import xplatj.javaplat.partic2.util.OneArgRunnable;

import java.io.IOException;
import java.util.HashSet;

public class RuntimeBridgeUtils {
    public static RpcExtendClient1 rtb;
    public static String RuntimeBridgePipeServerId="/pxprpc/runtime_bridge/java/0";
    static Object javaPipeServeMutex=new Object();
    public static PipeServer serv=null;
    public static HashSet<AbstractIo> acceptedConnection;
    public static RpcExtendClient1 client;
    public static RpcExtendClientCallable fServe;
    public static RpcExtendClientCallable fAccept;
    public static RpcExtendClientCallable fIo2RawAddr;
    public static RpcExtendClientCallable fVarGet;
    public static RpcExtendClientCallable fVarSet;
    public static RpcExtendClientCallable fVarOnChange;
    public synchronized static void ensureInit(){
        if(client==null){
            Io io = null;
            for(int i1=0;i1<10;i1++){
                io=Pipe.connect("/pxprpc/runtime_bridge/0");
                if(io!=null){
                    break;
                }
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {}
            }
            if(io==null){
                throw new RuntimeException("Failed to connect /pxprpc/runtime_bridge/0");
            }
            ClientContext client1 = new ClientContext();
            client1.backend1(io);
            client=new RpcExtendClient1(client1);
            try {
                client.init();
                fServe = client.getFunc("pxprpc_pipe_pp.serve").typedecl("s->o");
                fAccept = client.getFunc("pxprpc_pipe_pp.accept").typedecl("o->o");
                fIo2RawAddr=client.getFunc("pxprpc_pp.io_to_raw_addr").typedecl("o->l");
                fVarGet=client.getFunc("pxprpc_rtbridge.variable_get").typedecl("s->s");
                fVarSet=client.getFunc("pxprpc_rtbridge.variable_set").typedecl("ss->");
                fVarOnChange=client.getFunc("pxprpc_rtbridge.variable_on_change").typedecl("->o");
            } catch (IOException e) {
                throw new RuntimeException(e);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }
    public static String varGet(String name) throws IOException, InterruptedException {
        ensureInit();
        return (String)fVarGet.callBlock(name)[0];
    }
    public static void varSet(String name,String value) throws IOException, InterruptedException {
        ensureInit();
        fVarSet.callBlock(name,value);
    }
    public static RpcExtendClientCallable varOnChange() throws IOException, InterruptedException {
        ensureInit();
        RpcExtendClientObject ed = (RpcExtendClientObject)fVarOnChange.callBlock()[0];
        RpcExtendClientCallable ed2 = ed.asCallable();
        ed2.typedecl("->s");
        return ed2;
    }

    public static void registerJavaPipeServer() throws IOException {
        synchronized (javaPipeServeMutex){
            if(serv==null){
                ensureInit();
                Io testConn=Pipe.connect(RuntimeBridgePipeServerId);
                if(testConn!=null){
                    testConn.close();
                    throw new IOException("server name already used.");
                }
                serv=new PipeServer(RuntimeBridgePipeServerId);
                serv.serve();
                serv.accept(new OneArgRunnable<AbstractIo>() {
                    @Override
                    public void run(final AbstractIo conn) {
                        new Thread(new Runnable() {
                            @Override
                            public void run() {
                                acceptedConnection.add(conn);
                                try {
                                    ServerContext servCtx = new ServerContext();
                                    servCtx.init(conn, DefaultFuncMap.getDefault());
                                    servCtx.serve();
                                } catch (Exception e) {
                                } finally {
                                    conn.close();
                                    acceptedConnection.remove(conn);
                                }
                            }
                        }).start();
                    }
                });
            }
        }
    }
}
