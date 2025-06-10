package pxprpc.runtimebridge;

import pxprpc.base.AbstractIo;
import pxprpc.base.ServerContext;
import pxprpc.extend.DefaultFuncMap;

import java.io.IOException;

public class RuntimeBridgeUtils {
    public static String RuntimeBridgePipeServerId="/pxprpc/runtime_bridge_java/0";
    static Object javaPipeServeMutex=new Object();
    public static PipeServer serv=null;
    public static void registerJavaPipeServer() throws IOException {
        synchronized (javaPipeServeMutex){
            if(serv==null){
                PipeServer.ensureInit();
                Io testConn=Pipe.connect(RuntimeBridgePipeServerId);
                if(testConn!=null){
                    testConn.close();
                    throw new IOException("server name already registered.");
                }
                serv=new PipeServer("/pxprpc/runtime_bridge/java/0");
                try{
                    while(true){
                        AbstractIo io1=serv.acceptBlock();
                        if(io1==null)break;
                        ServerContext servCtx = new ServerContext();
                        servCtx.init(io1, DefaultFuncMap.getDefault());
                        new Thread(new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    servCtx.serve();
                                }catch(Exception ex){};
                            }
                        }).start();
                    }
                }catch(RuntimeException ex){};
            }
        }
    }
}
