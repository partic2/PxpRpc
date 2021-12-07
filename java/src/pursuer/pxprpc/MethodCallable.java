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
	public void readParameter(PxpRequest req) throws IOException {
		ServerContext c = req.context;
		final int thisObj=c.readInt32();
		final Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=null;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(c,argList[i]);
		}
		req.parameter=new Object[] {thisObj,args};
	}

	@Override
	public void call(PxpRequest req,AsyncReturn<Object> asyncRet) throws IOException {
		ServerContext ctx = req.context;
		try {
			Object result=null;
			Object[] args = (Object[])((Object[])req.parameter)[1];
			if(firstInputParamIndex>=1) {
				args[0]=asyncRet;
			}
			result=method.invoke(ctx.refSlots[(Integer)((Object[])req.parameter)[0]],args);
			if(firstInputParamIndex==0) {
				asyncRet.result(result);
			}
		} catch (Exception e) {
			asyncRet.result(e);
		} 
	}

}
