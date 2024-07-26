package pxprpc.extend;

import java.io.IOException;

public class RpcExtendClientObject {
    public RpcExtendClient1 client;
    public int value;
    RpcExtendClientObject(RpcExtendClient1 client,int value){
        this.client=client;
        this.value=value;
    }
    public void free() {
        if (this.value != -1) {
            int sid=this.client.allocSid();
            try{
                try {
                    this.client.conn.freeRef(new int[]{this.value},sid);
                } catch (Exception e) {
                }
            }finally{
                this.client.freeSid(this.value);
            }
        }
    }
    /* after return, "this" object are invalid*/
    public RpcExtendClientCallable asCallable(){
        int t1=this.value;
        this.value=-1;
        return new RpcExtendClientCallable(this.client,t1);
    }
}
