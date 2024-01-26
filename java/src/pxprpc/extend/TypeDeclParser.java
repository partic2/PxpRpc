package pxprpc.extend;

import pxprpc.base.PxpRef;
import pxprpc.base.Serializer2;
import pxprpc.base.ServerContext;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;

public class TypeDeclParser {

    public TypeDeclParser(ServerContext boundContext){
        this.boundContext=boundContext;
    }
    public static char[] parseDeclText(String declText){
        return declText.toCharArray();
    }

    public PxpRef allocRefFor(Object obj){
        PxpRef ref = boundContext.allocRef();
        if(obj instanceof Closeable){
            ref.set(obj,(Closeable) obj);
        }else{
            ref.set(obj,null);
        }
        return ref;
    }
    public static char jtypeToValueInfo(Class<?> jtype){
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
        }else {
            if (jtype.equals(Integer.class)) {
                return 'i';
            } else if (jtype.equals(Long.class)) {
                return 'l';
            } else if (jtype.equals(Float.class)) {
                return 'f';
            } else if (jtype.equals(Double.class)) {
                return 'd';
            } else if (jtype.equals(ByteBuffer.class)) {
                return 'b';
            } else if (jtype.equals(String.class)) {
                return 's';
            } else if (jtype.equals(Boolean.class)) {
                return 'c';
            } else if (jtype.equals(Void.class)) {
            }else{
                return 'o';
            }
        }
        return 0;
    }
    public static String formatDeclText(char[] vis){
        return new String(vis);
    }
    public Object unserializeValue(char vi, Serializer2 ser) {
        Object obj;
        int addr=-1;
        switch(vi) {
            //primitive type
            case 'c': //boolean
                obj = ser.getVarint()!=0;
                break;
            case 'i': //int
                obj = ser.getInt();
                break;
            case 'l': //long
                obj = ser.getLong();
                break;
            case 'f': //float
                obj = ser.getFloat();
                break;
            case 'd': //double
                obj = ser.getDouble();
                break;
            //reference type
            case 'o':
                addr=ser.getInt();
                if(addr!=-1){
                    obj = boundContext.getRef(addr).get();
                }else{
                    obj=null;
                }
                break;
            case 'b':
                //ByteBuffer
                obj = ser.getBytes();
                break;
            case 's':
                //String
                obj = ser.getString();
                break;
            default :
                throw new UnsupportedOperationException();
        }
        return obj;
    }
    public Object[] unserializeMultivalue(char[] vis, Serializer2 ser){
        ArrayList objs=new ArrayList<Object>();
        for(char vi:vis){
            objs.add(unserializeValue(vi,ser));
        }
        return objs.toArray(new Object[0]);
    }
    public void serializeMultivalue(char[] vis, Serializer2 ser,Object[] objs){
        for(int i=0;i<vis.length;i++) {
            char vi=vis[i];
            Object obj=objs[i];
            serializeValue(vi,ser,obj);
        }
    }
    public void serializeValue(char vi, Serializer2 ser,Object obj){
        switch (vi) {
            //primitive type
            //boolean
            case 'c':
                ser.putVarint((Boolean) obj ? 1 : 0);
                break;
            //int
            case 'i':
                ser.putInt((Integer) obj);
                break;
            //long
            case 'l':
                ser.putLong((Long) obj);
                break;
            //float
            case 'f':
                ser.putFloat((Float) obj);
                break;
            //double
            case 'd':
                ser.putDouble((Double) obj);
                break;
            //reference type
            case 'o':
                if(obj!=null){
                    ser.putInt(allocRefFor(obj).index);
                }else{
                    ser.putInt(-1);
                }
                break;
            case 'b':
                ByteBuffer b2 = (ByteBuffer) obj;
                ser.putBytes(b2);
                break;
            case 's':
                ser.putString((String) obj);
                break;
            default:
                throw new UnsupportedOperationException();
        }
    }
}
