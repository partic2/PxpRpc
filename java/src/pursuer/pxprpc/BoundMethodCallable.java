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
	public void call(final AsyncReturn<Object> asyncRet) throws IOException {
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
					result=method.invoke(boundObj, args);
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
