package pxprpc.extend;

import pxprpc.base.FuncMap;
import pxprpc.base.PxpCallable;
import pxprpc.base.Utils;

import java.util.HashMap;
import java.util.Map;

public class DefaultFuncMap implements FuncMap {
    public static Map<String,Object> registered=new HashMap<>();
    public static BuiltInFuncList builtIn=new BuiltInFuncList();

    public static DefaultFuncMap defaultInstance=new DefaultFuncMap();
    public static FuncMap getDefault(){
        if(!registered.containsKey("builtin")){
            registered.put("builtin", builtIn);
        }
        if(!registered.containsKey("pxprpc_pp")){
            registered.put("pxrpc_pp",new PxprpcPPFuncList());
        }
        return defaultInstance;
    }
    @Override
    public PxpCallable get(String name) {
        PxpCallable found = null;
        if(registered.containsKey(name)){
            found=(PxpCallable) registered.get(name);
        }
        if(found==null){
            int namespaceDelim = name.lastIndexOf(".");
            String namespace = name.substring(0, namespaceDelim);
            String func = name.substring(namespaceDelim + 1);
            Object obj = registered.get(namespace);
            if (obj != null) {
                found = builtIn.getBoundMethod(obj, func);
            }
        }
        return found;
    }
}
