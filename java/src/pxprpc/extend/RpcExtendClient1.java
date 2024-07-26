package pxprpc.extend;

import pxprpc.base.ClientContext;

import java.io.IOException;
import java.util.HashSet;
import java.util.concurrent.ConcurrentHashMap;

public class RpcExtendClient1 {
    protected ConcurrentHashMap<Integer,Object> __usedSid=new ConcurrentHashMap<>();

    protected int __sidStart = 1;
    protected int __sidEnd = 0xffff;
    protected int __nextSid=__sidStart;
    public ClientContext conn;

    public RpcExtendClient1(ClientContext conn) {
        this.conn=conn;
    }
    public String serverName;
    public RpcExtendClient1 init() throws IOException, InterruptedException {
        this.conn.run();
        for(String item :this.conn.getInfo(0x100).split("\n")){
            if(item.indexOf(':')>=0){
                String[] kv=item.split(":");
                if(kv[0].equals("server name")){
                    this.serverName=kv[1];
                }
            }
        }
        return this;
    }

    public int allocSid() {
        boolean reachEnd = false;
        while (this.__usedSid.contains(this.__nextSid)) {
            this.__nextSid += 1;
            if (this.__nextSid >= this.__sidEnd) {
                if (reachEnd) {
                    throw new RuntimeException("No sid available");
                } else {
                    reachEnd = true;
                    this.__nextSid = this.__sidStart;
                }
            }
        }

        int t1 = this.__nextSid;
        this.__nextSid += 1;
        if(this.__nextSid>=this.__sidEnd){
            this.__nextSid=this.__sidStart;
        }
        this.__usedSid.put(t1,0);
        return t1;
    }
    public void freeSid(int index) {
        this.__usedSid.remove(index);
    }
    public RpcExtendClientCallable getFunc(String name) throws IOException, InterruptedException {
        int sid=this.allocSid();
        try{
            int index=this.conn.getFunc(name,sid);
            if(index==-1)return null;
            return new RpcExtendClientCallable(this, index);
        }finally{
            this.freeSid(sid);
        }
    }
    public void close() throws IOException {
        this.conn.close(100);
    }
}
