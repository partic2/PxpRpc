package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.Method;

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
	public void call(final AsyncReturn<Object> asyncRet) throws IOException {
		final int thisObj=readInt32();
		final Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=asyncRet;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(argList[i]);
		}
		ctx.executor.execute(new Runnable() {
			@Override
			public void run() {
				try {
					Object result=null;
					result=method.invoke(ctx.refSlots.get(thisObj), args);
					if(firstInputParamIndex==0) {
						asyncRet.result(result);
					}
				} catch (Exception e) {
					asyncRet.result(e);
				} 
			}
		});
	}

}
