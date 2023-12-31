package pursuer.pxprpc;

import java.io.IOException;
import java.util.LinkedList;
import java.util.Queue;

public class EventDispatcher extends CommonCallable {
	public EventDispatcher() {
		this.tResult=new char[]{'o'};
	}
	public EventDispatcher setEventType(Class<?> c){
		this.tResult[0]=javaTypeToSwitchId(c);
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

	public void call(PxpRequest req, AsyncReturn<Object> asyncRet) throws IOException {
		receivers.offer(asyncRet);
	}
}
