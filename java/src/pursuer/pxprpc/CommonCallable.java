package pursuer.pxprpc;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;

public abstract class CommonCallable implements PxpCallable{
    public char[] tParam=new char[0];
    public char[] tResult=new char[0];

    public void readParameter(PxpRequest req) throws IOException {
        ByteChannel chan=req.getChan();
        int len=Utils.readInt32(chan);
        ByteBuffer buf = ByteBuffer.allocate(len & 0x7fffffff);
        Utils.readf(chan,buf);
        final Object[] args=new Object[tParam.length];
        if(tParam.length==1 && tParam[0]=='b'){
            args[0]=buf.array();
        }else{
            Serializer2 ser = new Serializer2().prepareUnserializing(buf);
            for(int i=0;i<tParam.length;i++) {
                args[i]=readNext(req,tParam[i],ser);
            }
        }
        req.parameter=args;
    }


    public void writeResult(PxpRequest req) throws IOException {
        Object obj = req.result;
        PxpChannel chan = req.getChan();
        Serializer2 ser = new Serializer2().prepareSerializing(32);
        ByteBuffer buf;
        if (obj instanceof Exception) {
            buf = ser.putString(((Exception) obj).getMessage()).build();
            Utils.writeInt32(chan, 0x80000000 | buf.remaining());
            Utils.writef(chan, buf);
        } else {
            if(tResult.length==1 && tResult[0]=='b'){
                buf=ByteBuffer.wrap((byte[])obj);
                Utils.writeInt32(chan,buf.remaining());
                Utils.writef(chan,buf);
            }else{
                if(tResult.length==1){
                    writeNext(req,tResult[0],ser,obj);
                }else{
                    Object[] multi = null;
                    if(tResult.length>1){
                        multi=(Object[]) obj;
                    }
                    for(int i=0;i<tResult.length;i++){
                        writeNext(req,tResult[i],ser,multi[i]);
                    }
                }
                buf = ser.build();
                Utils.writeInt32(chan,buf.remaining());
                Utils.writef(chan,buf);
            }
        }
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

    public static Object readNext(PxpRequest req, char switchId,Serializer2 ser) throws IOException {
        ByteChannel chan=req.getChan();
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
                return req.context.refSlots[addr].get();
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

    public static void writeNext(PxpRequest req, char switchId,Serializer2 ser,Object obj) throws IOException {
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
                ser.putInt(req.destAddr);
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
