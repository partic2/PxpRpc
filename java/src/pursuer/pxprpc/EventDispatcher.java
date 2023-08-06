package pursuer.pxprpc;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;

public class EventDispatcher implements PxpCallable {

	public EventDispatcher() {
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
		if(Exception.class.isInstance(req.result)) {
			req.context.writeInt32(1);
		}else {
			req.context.writeInt32(0);
		}
	}
}
