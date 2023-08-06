package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class MethodCallable implements PxpCallable{
	
	public Method method;
	
	public int[] argList;
	public int returnType;
	
	protected int firstInputParamIndex=0;
	
	public void writeResult(PxpRequest req) throws IOException{
		ServerContext ctx = req.context;
		Object obj=req.result;
		switch(returnType) {
		//primitive type 
			//boolean
		case 1:
			ctx.writeInt32((Boolean)obj?1:0);
			break;
			//int
		case 2:
			ctx.writeInt32((Integer)obj);
			break;
			//long
		case 3:
			ctx.writeInt64((Long)obj);
			break;
			//float
		case 4:
			ctx.writeFloat32((Float)obj);
			break;
			//double
		case 5:
			ctx.writeFloat64((Double)obj);
			break;
		//reference type
		case 6:
		case 7:
		case 8:
			if(Exception.class.isInstance(req.result)) {
				ctx.writeInt32(1);
			}else {
				ctx.writeInt32(0);
			}
			break;
		default :
			throw new UnsupportedOperationException();
		}
		
	}
	
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
	

	public int javaTypeToSwitchId(Class<?> jtype) {
		if(jtype.isPrimitive()) {
			String name = jtype.getName();
			if(name.equals("boolean")) {
				return 1;
			}else if(name.equals("int")) {
				return 2;
			}else if(name.equals("long")) {
				return 3;
			}else if(name.equals("float")) {
				return 4;
			}else if(name.equals("double")) {
				return 5;
			}else{
				return 6;
			}
		}else{
			if(jtype.equals(byte[].class)) {
				return 7;
			}else if(jtype.equals(String.class)) {
				return 8;
			}
			return 6;		
		}
	}
	
	public Object readNext(ServerContext ctx,int switchId) throws IOException {
		switch(switchId) {
		//primitive type 
		case 1: //boolean
			return ctx.readInt32()!=0;
		case 2: //int
			return ctx.readInt32();
		case 3: //long
			return ctx.readInt64();
		case 4: //float
			return ctx.readFloat32();
		case 5: //double
			return ctx.readFloat64();
		//reference type
		case 6:
			int addr=ctx.readInt32();
			return ctx.refSlots[addr].get();
		case 7:
			//byte[]
			return ctx.readNextBytes();
		case 8:
			//String
			return ctx.readNextString();
		default :
			throw new UnsupportedOperationException();
		}
	}


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
		} catch (InvocationTargetException e) {
			asyncRet.result(e.getCause());
		} catch (IllegalAccessException e) {
			asyncRet.result(e);
		}
	}

}
