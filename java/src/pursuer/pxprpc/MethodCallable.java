package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.Future;

public class MethodCallable extends AbstractCallable{
	
	public Method method;
	
	public int[] argList;
	public int returnType;
	
	protected int firstInputParamIndex=0;
	
	public MethodCallable(Method method) {
		this.method=method;
		Class<?>[] params = method.getParameterTypes();
		argList=new int[params.length];
		if(params.length>0&&params[0]==AsyncReturn.class) {
			firstInputParamIndex=1;
		}
		for(int i=firstInputParamIndex;i<params.length;i++) {
			Class<?> pc = params[i];
			argList[i]=javaTypeToSwitchId(pc);
		}
		returnType=javaTypeToSwitchId(method.getReturnType());
		
	}
	
	@Override
	public void call(AsyncReturn<Object> asyncRet) throws IOException {
		int thisObj=readInt32();
		Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=asyncRet;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(argList[i]);
		}
		Object result=null;
		try {
			result=method.invoke(ctx.refSlots.get(thisObj), args);
		} catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
		}
		if(firstInputParamIndex==0) {
			asyncRet.result(result);
		}
	}

}
