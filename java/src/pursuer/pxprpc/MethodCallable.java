package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;

public class MethodCallable implements PxpCallable{

	public Method method;

	public char[] argList;
	public char returnType;

	protected int firstInputParamIndex=0;

	public void writeResult(PxpRequest req) throws IOException {
        Object obj = req.result;
        PxpChannel chan = req.getChan();
        Serializer2 ser = new Serializer2().prepareSerializing(32);
        ByteBuffer buf;
        if (obj instanceof Exception) {
            buf = ser.putString(((Exception) obj).getMessage()).build();
            Utils.writeInt32(chan, 0x80000000 | buf.remaining());
            Utils.writef(chan, buf);
        } else {
			writeNext(req,returnType,ser,obj);
            buf = ser.build();
			Utils.writeInt32(chan,buf.remaining());
			Utils.writef(chan,buf);
        }
    }

	public MethodCallable(Method method) {
		this.method=method;
		Class<?>[] paramsType = method.getParameterTypes();
		argList=new char[paramsType.length];

		if (paramsType.length>0 && (paramsType[0] == AsyncReturn.class || paramsType[0] == PxpRequest.class)) {
			firstInputParamIndex = 1;
			returnType = javaTypeToSwitchId(method.getReturnType());
		}else if(returnType==0){
			returnType = javaTypeToSwitchId(method.getReturnType());
		}

		for(int i=firstInputParamIndex;i<paramsType.length;i++) {
			Class<?> pc = paramsType[i];
			argList[i]=javaTypeToSwitchId(pc);
		}

	}


	public void readParameter(PxpRequest req) throws IOException {
		ByteChannel chan=req.getChan();
		int len=Utils.readInt32(chan);
		ByteBuffer buf = ByteBuffer.allocate(len & 0x7fffffff);
		Utils.readf(chan,buf);
		Serializer2 ser = new Serializer2().prepareUnserializing(buf);
		if((len&0x80000000)!=0){
			//discard meta info
			ser.getString();
		}
		final int thisObj=ser.getInt();
		final Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=null;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(req,argList[i],ser);
		}
		req.parameter=new Object[] {thisObj,args};
	}


	public static char javaTypeToSwitchId(Class<?> jtype) {
		if(jtype.isPrimitive()) {
			String name = jtype.getName();
			if(name.equals("boolean")) {
				return 'c';
			}else if(name.equals("int")) {
				return 'i';
			}else if(name.equals("long")) {
				return 'l';
			}else if(name.equals("float")) {
				return 'f';
			}else if(name.equals("double")) {
				return 'd';
			}else{
				return 'o';
			}
		}else{
			if(jtype.equals(byte[].class)) {
				return 'b';
			}else if(jtype.equals(String.class)) {
				return 's';
			}else if(jtype.equals(Void.class)){
				//void
				return 0;
			}
			return 'o';
		}
	}

	public static Object readNext(PxpRequest req, char switchId,Serializer2 ser) throws IOException {
		ByteChannel chan=req.getChan();
		int addr=-1;
		switch(switchId) {
			//primitive type
			case 'c': //boolean
				return ser.getVarint()!=0;
			case 'i': //int
				return ser.getInt();
			case 'l': //long
				return ser.getLong();
			case 'f': //float
				return ser.getFloat();
			case 'd': //double
				return ser.getDouble();
			//reference type
			case 'o':
				addr=ser.getInt();
				return req.context.refSlots[addr].get();
			case 'b':
				//byte[]
				return ser.getBytes();
			case 's':
				//String
				return ser.getString();
			default :
				throw new UnsupportedOperationException();
		}
	}

	public static void writeNext(PxpRequest req, char switchId,Serializer2 ser,Object obj) throws IOException {
		switch (switchId) {
			//primitive type
			//boolean
			case 'c' :
				ser.putVarint((boolean) obj ? 1 : 0);
				break;
			//int
			case 'i' :
				ser.putInt((int) obj);
				break;
			//long
			case 'l' :
				ser.putLong((long) obj);
				break;
			//float
			case 'f' :
				ser.putFloat((float) obj);
				break;
			//double
			case 'd' :
				ser.putDouble((double) obj);
				break;
			//reference type
			case 'o' :
				ser.putInt(req.destAddr);
				break;
			case 'b' :
				byte[] b2 = (byte[]) obj;
				ser.putBytes(b2, 0, b2.length);
				break;
			case 's' :
				ser.putString((String) obj);
				break;
			default:
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
