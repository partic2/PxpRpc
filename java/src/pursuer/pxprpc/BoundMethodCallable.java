package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.Method;

public class BoundMethodCallable extends MethodCallable{

	public Object boundObj;
	public BoundMethodCallable(Method method,Object boundObj) {
		super(method);
		this.boundObj=boundObj;
	}
	
	@Override
	public void readParameter(PxpRequest req) throws IOException {
		ServerContext c = req.context;
		final Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=null;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(c,argList[i]);
		}
		req.parameter=new Object[] {null,args};
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
			result=method.invoke(this.boundObj,args);
			if(firstInputParamIndex==0) {
				asyncRet.result(result);
			}
		} catch (Exception e) {
			asyncRet.result(e);
		} 
	}

}
