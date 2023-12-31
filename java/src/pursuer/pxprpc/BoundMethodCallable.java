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
	public void parseMethod() {
		if(!parseCustomTypeDecl()){
			Class<?>[] paramsType = method.getParameterTypes();
			if (paramsType.length>0 && (paramsType[0] == AsyncReturn.class || paramsType[0] == PxpRequest.class)) {
				firstInputParamIndex = 1;
			}
			tParam=new char[paramsType.length-firstInputParamIndex];
			if(method.getReturnType()!=Void.class){
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

	@Override
	public void call(PxpRequest req,AsyncReturn<Object> asyncRet) throws IOException {
		ServerContext ctx = req.context;
		try {
			Object result=null;
			Object[] args = (Object[])req.parameter;
			Object[] args2=new Object[firstInputParamIndex+args.length];
			System.arraycopy(args,0,args2,firstInputParamIndex,args.length);
			if(firstInputParamIndex==1){
				args2[0]=asyncRet;
			}
			result=method.invoke(this.boundObj,args2);
			if(firstInputParamIndex==0) {
				asyncRet.result(result);
			}
		} catch (InvocationTargetException e) {
			asyncRet.result(e.getCause());
		} catch (IllegalAccessException e) {
			asyncRet.result(e);
		}
	}

}
