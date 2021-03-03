package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.Future;

public class MethodCallable extends AbstractCallable{
	
	public Method method;
	
	public int[] argList;
	public int returnType;
	
	public MethodCallable(Method method) {
		this.method=method;
		Class<?>[] params = method.getParameterTypes();
		argList=new int[params.length];
		for(int i=0;i<params.length;i++) {
			Class<?> pc = params[i];
			argList[i]=javaTypeToSwitchId(pc);
		}
		returnType=javaTypeToSwitchId(method.getReturnType());
		
	}
	
	
	@Override
	public void call(AsyncReturn<Object> asyncRet) throws IOException {
		int thisObj=readInt32();
		Object[] args=new Object[argList.length];
		for(int i=0;i<argList.length;i++) {
			args[i]=readNext(argList[i]);
		}
		Object result=null;
		try {
			result=method.invoke(ctx.refSlots.get(thisObj), args);
		} catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
		}
		asyncRet.result(result);
	}

}
