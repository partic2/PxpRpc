package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;

public class EventDispatcher implements PxpCallable {

	public char eventType='o';
	public EventDispatcher() {
	}
	public EventDispatcher setEventType(Class<?> c){
		this.eventType=MethodCallable.javaTypeToSwitchId(c);
		return this;
	}
	protected Queue<AsyncReturn<Object>> receivers = new LinkedList<AsyncReturn<Object>>();

	public void fireEvent(Object evt) {
		AsyncReturn<Object> r = receivers.poll();
		if(r!=null) {
			r.result(evt);
		}
	}

	public void clearReceivers() {
		receivers.clear();
	}


	public void readParameter(PxpRequest req) throws IOException {
	}

	public void call(PxpRequest req, AsyncReturn<Object> asyncRet) throws IOException {
		receivers.offer(asyncRet);
	}

	@Override
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
			MethodCallable.writeNext(req,eventType,ser,obj);
			buf = ser.build();
			Utils.writeInt32(chan,buf.remaining());
			Utils.writef(chan,buf);
		}
	}
}
