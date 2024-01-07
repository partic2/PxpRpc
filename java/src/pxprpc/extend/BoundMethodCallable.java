package pxprpc.extend;

import pxprpc.base.PxpRequest;
import pxprpc.base.ServerContext;

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
	public void parseMethod() {
		if(!parseCustomTypeDecl()){
			Class<?>[] paramsType = method.getParameterTypes();
			if (paramsType.length>0 && (paramsType[0] == AsyncReturn.class || paramsType[0] == PxpRequest.class)) {
				firstInputParamIndex = 1;
			}
			tParam=new char[paramsType.length-firstInputParamIndex];
			if(method.getReturnType()!=void.class){
				tResult = new char[]{javaTypeToSwitchId(method.getReturnType())};
			}else{
				tResult=new char[0];
			}
			for(int i=firstInputParamIndex;i<paramsType.length;i++) {
				Class<?> pc = paramsType[i];
				tParam[i-firstInputParamIndex]=javaTypeToSwitchId(pc);
			}
		}
	}

	public void call(final PxpRequest req) throws IOException {
		ServerContext ctx = req.context;
		try {
			Object[] args=readParameter(req);
			Object[] args2=new Object[firstInputParamIndex+args.length];
			System.arraycopy(args,0,args2,firstInputParamIndex,args.length);
			if(firstInputParamIndex==1){
				args2[0]=new AsyncReturn<Object>() {
					@Override
					public void resolve(Object result) {
						BoundMethodCallable.this.writeResult(req,result);
						req.done();
					}
					@Override
					public void reject(Exception ex) {
						req.rejected=ex;
						req.done();
					}
				};
			}
			Object result=method.invoke(boundObj,args2);
			if(firstInputParamIndex==0) {
				this.writeResult(req,result);
				req.done();
			}
		} catch (InvocationTargetException e) {
			req.rejected=e.getCause();
			req.done();
		} catch (IllegalAccessException e) {
			req.rejected=e;
			req.done();
		}
	}

}
