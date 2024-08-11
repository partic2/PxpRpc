package pxprpc.extend;

import pxprpc.base.ClientContext;
import pxprpc.base.RemoteError;
import pxprpc.base.Serializer2;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class RpcExtendClientCallable extends RpcExtendClientObject {
    public char[] tParam=new char[0];
    public char[] tResult=new char[0];
    RpcExtendClientCallable(RpcExtendClient1 client, int value) {
        super(client, value);
    }
    public RpcExtendClientCallable typedecl(String decl){
        int delim=decl.indexOf("->");
        tParam=TypeDeclParser.parseDeclText(decl.substring(0,delim));
        tResult=TypeDeclParser.parseDeclText(decl.substring(delim+2));
        return this;
    }
    public static interface Ret{
        void cb(Object[] r,RemoteError err);
    }
    public void call(final Ret result,Object ...parameters) throws IOException {
        ByteBuffer buf;
        if(tParam.length==1 && tParam[0]=='b'){
            buf=(ByteBuffer)parameters[0];
        }else{
            Serializer2 ser = new Serializer2().prepareSerializing(32);
            new TableSerializer().bindSerializer(ser)
                    .bindContext(null,this.client).setColumnInfo2(tParam,null)
                    .putRowsData(Arrays.asList(new Object[][]{parameters}));
            buf = ser.build();
        }
        final int sid=this.client.allocSid();
        this.client.conn.call(this.value, buf, sid, new ClientContext.Ret() {
            @Override
            public void cb(ByteBuffer result2, RemoteError err) {
                RpcExtendClientCallable.this.client.freeSid(sid);
                if(err!=null){
                    result.cb(null,err);
                    return;
                }
                if(tResult.length==1 && tResult[0]=='b'){
                    result.cb(new Object[]{result2},null);
                }
                Serializer2 ser = new Serializer2().prepareUnserializing(result2);
                Object[] result3 = new TableSerializer().bindSerializer(ser)
                        .bindContext(null, RpcExtendClientCallable.this.client).setColumnInfo2(tResult, null)
                        .getRowsData(1).get(0);
                result.cb(result3,null);
            }
        });
    }
    public Object[] callBlock(Object... parameters) throws IOException, InterruptedException {
        ByteBuffer buf;
        if(tParam.length==1 && tParam[0]=='b'){
            buf=(ByteBuffer)parameters[0];
        }else{
            Serializer2 ser = new Serializer2().prepareSerializing(32);
            new TableSerializer().bindSerializer(ser)
                    .bindContext(null,this.client).setColumnInfo2(tParam,null)
                    .putRowsData(Arrays.asList(new Object[][]{parameters}));
            buf = ser.build();
        }
        int sid=this.client.allocSid();
        try{
            ByteBuffer result2 = this.client.conn.callBlock(this.value,buf,sid);
            if(tResult.length==1 && tResult[0]=='b'){
                return new Object[]{result2};
            }
            Serializer2 ser = new Serializer2().prepareUnserializing(result2);
            Object[] result3 = new TableSerializer().bindSerializer(ser)
                    .bindContext(null, RpcExtendClientCallable.this.client).setColumnInfo2(tResult, null)
                    .getRowsData(1).get(0);
            return result3;
        }finally {
            this.client.freeSid(sid);
        }
    }
}
