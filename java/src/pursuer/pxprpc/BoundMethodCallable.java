package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;

public class BoundMethodCallable extends MethodCallable{

	public Object boundObj;
	public BoundMethodCallable(Method method,Object boundObj) {
		super(method);
		this.boundObj=boundObj;
	}
	
	@Override
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
		final Object[] args=new Object[argList.length];
		if(firstInputParamIndex>=1) {
			args[0]=null;
		}
		for(int i=firstInputParamIndex;i<argList.length;i++) {
			args[i]=readNext(req,argList[i],ser);
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
				args[0]=req;
			}
			result=method.invoke(this.boundObj,args);
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
