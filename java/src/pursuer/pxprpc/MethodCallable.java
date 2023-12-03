package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.channels.ByteChannel;

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
				Utils.writeInt32(req.getChan(),(Boolean)obj?1:0);
				break;
			//int
			case 2:
				Utils.writeInt32(req.getChan(),(Integer)obj);
				break;
			//long
			case 3:
				Utils.writeInt64(req.getChan(),(Long)obj);
				break;
			//float
			case 4:
				Utils.writeFloat32(req.getChan(),(Float)obj);
				break;
			//double
			case 5:
				Utils.writeFloat64(req.getChan(),(Double)obj);
				break;
			//reference type
			case 6:
			case 7:
			case 8:
				if(Exception.class.isInstance(req.result)) {
					Utils.writeInt32(req.getChan(),1);
				}else {
					Utils.writeInt32(req.getChan(),0);
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
		if(params.length>0&&(params[0]==AsyncReturn.class||params[0]==PxpRequest.class)) {
			firstInputParamIndex=1;
		}
		for(int i=firstInputParamIndex;i<params.length;i++) {
			Class<?> pc = params[i];
			argList[i]=javaTypeToSwitchId(pc);
		}
		returnType=javaTypeToSwitchId(method.getReturnType());

	}


	public void readParameter(PxpRequest req) throws IOException {
		ByteChannel chan=req.getChan();
		final int thisObj=Utils.readInt32(chan);
		final Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=null;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(req,argList[i]);
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

	public Object readNext(PxpRequest req, int switchId) throws IOException {
		ByteChannel chan=req.getChan();
		int addr=-1;
		switch(switchId) {
			//primitive type
			case 1: //boolean
				return Utils.readInt32(chan)!=0;
			case 2: //int
				return Utils.readInt32(chan);
			case 3: //long
				return Utils.readInt64(chan);
			case 4: //float
				return Utils.readFloat32(chan);
			case 5: //double
				return Utils.readFloat64(chan);
			//reference type
			case 6:
				addr=Utils.readInt32(chan);
				return req.context.refSlots[addr].get();
			case 7:
				//byte[]
				addr=Utils.readInt32(chan);
				return req.context.getBytesAt(addr);
			case 8:
				//String
				addr=Utils.readInt32(chan);
				return req.context.getStringAt(addr);
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
				args[0]=req;
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
