package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;
import java.util.Arrays;

public class MethodCallable implements PxpCallable{

	public Method method;

	public char[] tParam;
	public char[] tResult;

	protected int firstInputParamIndex=0;



	public MethodCallable(Method method) {
		this.method=method;
		parseMethod();
	}

	public boolean parseCustomSignature(){
		MethodSignature customSign = method.getAnnotation(MethodSignature.class);
		if(customSign!=null){
			String sign=customSign.value()[0];
			int delim=sign.indexOf("->");
			tParam=sign.substring(0,delim).toCharArray();
			tResult=sign.substring(delim+2).toCharArray();
			return true;
		}else{
			return false;
		}
	}
	public void parseMethod(){
		if(!parseCustomSignature()){
			Class<?>[] paramsType = method.getParameterTypes();
			if (paramsType.length>0 && (paramsType[0] == AsyncReturn.class || paramsType[0] == PxpRequest.class)) {
				firstInputParamIndex = 1;
			}
			tParam=new char[paramsType.length+1-firstInputParamIndex];
			if(tResult==null){
				if(method.getReturnType()!=Void.class){
					tResult = new char[]{javaTypeToSwitchId(method.getReturnType())};
				}else{
					tResult=new char[0];
				}
			}
			tParam[0]='o';
			for(int i=firstInputParamIndex;i<paramsType.length;i++) {
				Class<?> pc = paramsType[i];
				tParam[i-firstInputParamIndex+1]=javaTypeToSwitchId(pc);
			}
		}
	}


	public void readParameter(PxpRequest req) throws IOException {
		ByteChannel chan=req.getChan();
		int len=Utils.readInt32(chan);
		ByteBuffer buf = ByteBuffer.allocate(len & 0x7fffffff);
		Utils.readf(chan,buf);
		final Object[] args=new Object[tParam.length];
		if(tParam.length==1 && tParam[0]=='b'){
			args[0]=buf.array();
		}else{
			Serializer2 ser = new Serializer2().prepareUnserializing(buf);
			for(int i=0;i<tParam.length;i++) {
				args[i]=readNext(req,tParam[i],ser);
			}
		}
		req.parameter=args;
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
			if(tResult.length==1 && tResult[0]=='b'){
				buf=ByteBuffer.wrap((byte[])obj);
				Utils.writeInt32(chan,buf.remaining());
				Utils.writef(chan,buf);
			}else{
				if(tResult.length==1){
					writeNext(req,tResult[0],ser,obj);
				}else{
					Object[] multi=(Object[]) obj;
					for(int i=0;i<tResult.length;i++){
						writeNext(req,tResult[i],ser,multi[i]);
					}
				}
				buf = ser.build();
				Utils.writeInt32(chan,buf.remaining());
				Utils.writef(chan,buf);
			}
		}
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
				ser.putVarint((Boolean) obj ? 1 : 0);
				break;
			//int
			case 'i' :
				ser.putInt((Integer) obj);
				break;
			//long
			case 'l' :
				ser.putLong((Long) obj);
				break;
			//float
			case 'f' :
				ser.putFloat((Float) obj);
				break;
			//double
			case 'd' :
				ser.putDouble((Double) obj);
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


}
