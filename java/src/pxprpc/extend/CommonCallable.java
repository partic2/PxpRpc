package pxprpc.extend;

import pxprpc.base.PxpCallable;
import pxprpc.base.PxpRequest;
import pxprpc.base.Serializer2;

import java.nio.ByteBuffer;
import java.util.Arrays;

public abstract class CommonCallable implements PxpCallable {
    public char[] tParam=new char[0];
    public char[] tResult=new char[0];

    public Object[] readParameter(PxpRequest req) {
        ByteBuffer buf = req.parameter;
        Object[] args=new Object[tParam.length];
        if(tParam.length==1 && tParam[0]=='b'){
            args[0]=buf;
        }else{
            Serializer2 ser = new Serializer2().prepareUnserializing(buf);
            args=new TableSerializer().bindSerializer(ser)
                    .bindContext(req.context,null).setColumnInfo2(tParam,null)
                    .getRowsData(1).get(0);
        }
        return args;
    }


    public void writeResult(PxpRequest req,Object result) {

        ByteBuffer buf;

        if(tResult.length==1 && tResult[0]=='b'){
            buf=(ByteBuffer)result;
        }else{
            Serializer2 ser = new Serializer2().prepareSerializing(32);
            Object[] rs;
            if(tResult.length<=1){
                rs=new Object[]{result};
            }else{
                rs=(Object[]) result;
            }
            new TableSerializer().bindSerializer(ser)
                    .bindContext(req.context,null).setColumnInfo2(tResult,null)
                    .putRowsData(Arrays.asList(new Object[][]{rs}));
            buf = ser.build();
        }
        req.result=new ByteBuffer[]{buf};

    }

}
