package pursuer.pxprpc;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.Map;

public class BuiltInFuncList {
	public String asString(byte[] utf8) {
		try {
			return new String(utf8,"utf-8");
		} catch (UnsupportedEncodingException e) {
			return null;
		}
	}
	public String anyToString(Object obj){
		return obj.toString();
	}
	public AbstractCallable getMethod(Object obj,String methodName){
		Method found=null;
		for(Method method:obj.getClass().getMethods()) {
			if(method.getName().equals(methodName)) {
				found=method;
				break;
			}
		}
		return new MethodCallable(found);
	}
	public AbstractCallable getBoundMethod(Object obj,String methodName){
		Method found=null;
		for(Method method:obj.getClass().getMethods()) {
			if(method.getName().equals(methodName)) {
				found=method;
				break;
			}
		}
		return new BoundMethodCallable(found,obj);
	}
	public String checkException(Object obj) {
		if(obj!=null&&Exception.class.isInstance(obj)){
			return obj.toString();
		}else{
			return "";
		}
	}
	public Class<?> findClass(String name){
		try {
			return Class.forName(name);
		} catch (ClassNotFoundException e) {
			return null;
		}
	}
	public Object newObject(Class<Object> cls){
		try {
			return cls.getConstructor().newInstance();
		} catch (Exception e) {
			return e;
		}
	}
	
	public int listLength(List<Object> array) {
		return array.size();
	}
	public Object listElemAt(List<Object> array,int index) {
		return array.get(index);
	}
	public void listAdd(List<Object> array,Object obj) {
		array.add(obj);
	}
	public void listRemove(List<Object> array,int index) {
		array.remove(index);
	}
	public byte[] listBytesJoin(List<byte[]> array,byte[] sep) throws IOException {
		int size=array.size();
		if(size==0){
			return new byte[0];
		}
		ByteArrayOutputStream bais=new ByteArrayOutputStream();
		bais.write(array.get(0));
		for(int i1=1;i1<size;i1++){
			bais.write(sep);
			bais.write(array.get(i1));
		}
		return bais.toByteArray();
	}
	public String listStringJoin(List<String> array,String sep){
		int size=array.size();
		if(size==0){
			return "";
		}
		StringBuilder sb=new StringBuilder();
		sb.append(array.get(0));
		for(int i1=1;i1<size;i1++){
			sb.append(sep);
			sb.append(array.get(i1));
		}
		return sb.toString();
	}

}
