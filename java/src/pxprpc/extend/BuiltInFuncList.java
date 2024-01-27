package pxprpc.extend;

import pxprpc.base.PxpCallable;
import pxprpc.base.PxpRequest;
import pxprpc.base.Serializer2;
import pxprpc.base.Utils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

public class BuiltInFuncList {

	public String anyToString(Object obj){
		return obj.toString();
	}
	public PxpCallable getMethod(Object obj, String methodName){
		Method found=null;
		for(Method method:obj.getClass().getMethods()) {
			if(method.getName().equals(methodName)) {
				found=method;
				break;
			}
		}
		return new MethodCallable(found);
	}
	public PxpCallable getBoundMethod(Object obj,String methodName){
		Method found=null;
		for(Method method:obj.getClass().getMethods()) {
			if(method.getName().equals(methodName)) {
				found=method;
				break;
			}
		}
		if(found==null){
			return null;
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


	public List<Object> listNew(AsyncReturn<List<Object>> ret, ByteBuffer init){
		ret.resolve(new TableSerializer().load(init).toArray());
		return null;
	}
	public ByteBuffer listSerialize(AsyncReturn<ByteBuffer> ret,List<Object> objs){
		ret.resolve(new TableSerializer().fromArray(objs).build());
		return null;
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


	public ByteBuffer listStaticConstField(Class<?> cls) throws IllegalAccessException {
		ArrayList<String> hdrs = new ArrayList<String>();
		ArrayList<Object> row = new ArrayList<Object>();
		Field[] fields = cls.getFields();
		for(Field f:fields){
			if((f.getModifiers()& (Modifier.STATIC|Modifier.FINAL))==(Modifier.STATIC|Modifier.FINAL)){
				if(f.getType().isPrimitive()||f.getType().equals(String.class)){
					hdrs.add(f.getName());
					row.add(f.get(null));
				}
			}
		}
		return new TableSerializer().setHeader(null,hdrs.toArray(new String[0]))
				.addRow(row.toArray(new Object[0])).build();
	}

}
