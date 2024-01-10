package pxprpc.extend;

import pxprpc.base.PxpCallable;
import pxprpc.base.PxpRequest;
import pxprpc.base.Serializer2;

import java.nio.ByteBuffer;

public abstract class CommonCallable implements PxpCallable {
    public char[] tParam=new char[0];
    public char[] tResult=new char[0];

    public Object[] readParameter(PxpRequest req) {
        ByteBuffer buf = req.parameter;
        Object[] args=new Object[tParam.length];
        if(tParam.length==1 && tParam[0]=='b'){
            args[0]=buf.array();
        }else{
            Serializer2 ser = new Serializer2().prepareUnserializing(buf);
            args=new TypeDeclParser(req.context).unserializeMultivalue(tParam,ser);
        }
        return args;
    }


    public void writeResult(PxpRequest req,Object result) {

        ByteBuffer buf;

        if(tResult.length==1 && tResult[0]=='b'){
            buf=ByteBuffer.wrap((byte[])result);
        }else{
            Serializer2 ser = new Serializer2().prepareSerializing(32);
            Object[] rs;
            if(tResult.length<=1){
                rs=new Object[]{result};
            }else{
                rs=(Object[]) result;
            }
            new TypeDeclParser(req.context).serializeMultivalue(tResult,ser,rs);
            buf = ser.build();
        }
        req.result=new ByteBuffer[]{buf};

    }

    public static void writeNext(PxpRequest req, char switchId,Serializer2 ser,Object obj){

    }

}
