package pursuer.pxprpc;

import java.io.UnsupportedEncodingException;
import java.lang.reflect.Method;
import java.util.List;

public class BuiltInFuncList {
	public String asString(byte[] utf8) {
		try {
			return new String(utf8,"utf-8");
		} catch (UnsupportedEncodingException e) {
			return null;
		}
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
	public boolean isException(Object obj) {
		return obj.getClass().isInstance(Exception.class);
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
	public Object listIndexOf(List<Object> array,int index) {
		return array.get(index);
	}
	public void listAdd(List<Object> array,Object obj) {
		array.add(obj);
	}
	public void listRemove(List<Object> array,int index) {
		array.remove(index);
	}
}
