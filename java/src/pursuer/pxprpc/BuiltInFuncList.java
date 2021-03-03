package pursuer.pxprpc;

import java.io.UnsupportedEncodingException;
import java.lang.reflect.Method;

public class BuiltInFuncList {
	public String asString(byte[] utf8) {
		try {
			return new String(utf8,"utf-8");
		} catch (UnsupportedEncodingException e) {
			return null;
		}
	}
	public Method getMethod(Object obj,String methodName){
		Method found=null;
		for(Method method:obj.getClass().getMethods()) {
			if(method.getName().equals(methodName)) {
				found=method;
				break;
			}
		}
		return found;
	}
}
