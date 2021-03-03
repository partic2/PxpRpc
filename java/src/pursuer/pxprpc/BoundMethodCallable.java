package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class BoundMethodCallable extends MethodCallable{

	public Object boundObj;
	public BoundMethodCallable(Method method,Object boundObj) {
		super(method);
		this.boundObj=boundObj;
	}
	
	@Override
	public void call(AsyncReturn<Object> asyncRet) throws IOException {
		Object[] args=new Object[argList.length];
		for(int i=0;i<argList.length;i++) {
			args[i]=readNext(argList[i]);
		}
		Object result=null;
		try {
			result=method.invoke(boundObj, args);
		} catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
			e.printStackTrace();
		}
		asyncRet.result(result);
	}

}
