package pxprpc.extend;

import pxprpc.base.PxpRef;
import pxprpc.base.Serializer2;
import pxprpc.base.ServerContext;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;

public class TypeDeclParser {

    public static char[] parseDeclText(String declText){
        return declText.toCharArray();
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
            } else if (ByteBuffer.class.isAssignableFrom(jtype)) {
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

}
