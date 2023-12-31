package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class MethodCallable extends CommonCallable {

	public Method method;

	protected int firstInputParamIndex=0;



	public MethodCallable(Method method) {
		this.method=method;
		parseMethod();
	}

	public boolean parseCustomTypeDecl(){
		MethodTypeDecl typeDecl= method.getAnnotation(MethodTypeDecl.class);
		if(typeDecl!=null){
			String decl=typeDecl.value()[0];
			int delim=decl.indexOf("->");
			tParam=decl.substring(0,delim).toCharArray();
			tResult=decl.substring(delim+2).toCharArray();
			return true;
		}else{
			return false;
		}
	}
	public void parseMethod(){
		if(!parseCustomTypeDecl()){
			Class<?>[] paramsType = method.getParameterTypes();
			if (paramsType.length>0 && (paramsType[0] == AsyncReturn.class || paramsType[0] == PxpRequest.class)) {
				firstInputParamIndex = 1;
			}
			tParam=new char[paramsType.length+1-firstInputParamIndex];
			if(method.getReturnType()!=Void.class){
				tResult = new char[]{javaTypeToSwitchId(method.getReturnType())};
			}else{
				tResult=new char[0];
			}
			tParam[0]='o';
			for(int i=firstInputParamIndex;i<paramsType.length;i++) {
				Class<?> pc = paramsType[i];
				tParam[i-firstInputParamIndex+1]=javaTypeToSwitchId(pc);
			}
		}
	}


	public void call(PxpRequest req,AsyncReturn<Object> asyncRet) throws IOException {
		ServerContext ctx = req.context;
		try {
			Object result=null;
			Object[] args = (Object[])req.parameter;
			Object[] args2=new Object[firstInputParamIndex+args.length-1];
			System.arraycopy(args,1,args2,firstInputParamIndex,args.length-1);
			if(firstInputParamIndex==1){
				args2[0]=asyncRet;
			}
			result=method.invoke(args[0],args2);
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
