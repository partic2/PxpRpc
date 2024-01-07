package pxprpc.extend;

import pxprpc.base.PxpCallable;
import pxprpc.base.PxpRef;
import pxprpc.base.PxpRequest;
import pxprpc.base.Serializer2;

import java.nio.ByteBuffer;

public abstract class CommonCallable implements PxpCallable {
    public char[] tParam=new char[0];
    public char[] tResult=new char[0];

    public Object[] readParameter(PxpRequest req) {
        ByteBuffer buf = req.parameter;
        final Object[] args=new Object[tParam.length];
        if(tParam.length==1 && tParam[0]=='b'){
            args[0]=buf.array();
        }else{
            Serializer2 ser = new Serializer2().prepareUnserializing(buf);
            for(int i=0;i<tParam.length;i++) {
                args[i]=readNext(req,tParam[i],ser);
            }
        }
        return args;
    }


    public void writeResult(PxpRequest req,Object result) {
        Serializer2 ser = new Serializer2().prepareSerializing(32);
        ByteBuffer buf;
        if (result instanceof Exception) {
            buf = ser.putString(((Exception) result).getMessage()).build();
        } else {
            if(tResult.length==1 && tResult[0]=='b'){
                buf=ByteBuffer.wrap((byte[])result);
            }else{
                if(tResult.length==1){
                    writeNext(req,tResult[0],ser,result);
                }else{
                    Object[] multi = null;
                    if(tResult.length>1){
                        multi=(Object[]) result;
                    }
                    for(int i=0;i<tResult.length;i++){
                        writeNext(req,tResult[i],ser,multi[i]);
                    }
                }
                buf = ser.build();
            }
        }
        req.result=new ByteBuffer[]{buf};

    }


    public static char javaTypeToSwitchId(Class<?> jtype) {
        if(jtype.isPrimitive()) {
            String name = jtype.getName();
            if(name.equals("boolean")) {
                return 'c';
            }else if(name.equals("int")) {
                return 'i';
            }else if(name.equals("long")) {
                return 'l';
            }else if(name.equals("float")) {
                return 'f';
            }else if(name.equals("double")) {
                return 'd';
            }else{
                return 'o';
            }
        }else{
            if (jtype.equals(Integer.class)) {
                return 'i';
            } else if (jtype.equals(Long.class)) {
                return 'l';
            } else if (jtype.equals(Float.class)) {
                return 'f';
            } else if (jtype.equals(Double.class)) {
                return 'd';
            } else if (jtype.equals(byte[].class)) {
                return 'b';
            } else if (jtype.equals(String.class)) {
                return 's';
            } else if (jtype.equals(Boolean.class)) {
                return 'c';
            } else if (jtype.equals(Void.class)) {
                return 0;
            }
            return 'o';
        }
    }

    public static Object readNext(PxpRequest req, char switchId,Serializer2 ser) {
        int addr=-1;
        switch(switchId) {
            //primitive type
            case 'c': //boolean
                return ser.getVarint()!=0;
            case 'i': //int
                return ser.getInt();
            case 'l': //long
                return ser.getLong();
            case 'f': //float
                return ser.getFloat();
            case 'd': //double
                return ser.getDouble();
            //reference type
            case 'o':
                addr=ser.getInt();
                return req.context.refPool[addr].get();
            case 'b':
                //byte[]
                return ser.getBytes();
            case 's':
                //String
                return ser.getString();
            default :
                throw new UnsupportedOperationException();
        }
    }

    public static void writeNext(PxpRequest req, char switchId,Serializer2 ser,Object obj){
        switch (switchId) {
            //primitive type
            //boolean
            case 'c' :
                ser.putVarint((Boolean) obj ? 1 : 0);
                break;
            //int
            case 'i' :
                ser.putInt((Integer) obj);
                break;
            //long
            case 'l' :
                ser.putLong((Long) obj);
                break;
            //float
            case 'f' :
                ser.putFloat((Float) obj);
                break;
            //double
            case 'd' :
                ser.putDouble((Double) obj);
                break;
            //reference type
            case 'o' :
                PxpRef ref2 = req.context.allocRef();
                ref2.set(obj);
                ser.putInt(ref2.index);
                break;
            case 'b' :
                byte[] b2 = (byte[]) obj;
                ser.putBytes(b2, 0, b2.length);
                break;
            case 's' :
                ser.putString((String) obj);
                break;
            default:
                throw new UnsupportedOperationException();
        }
    }

}
