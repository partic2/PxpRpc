package pursuer.pxprpc;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;

public class EventDispatcher extends AbstractCallable {

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


	@Override
	public void readParameter(PxpRequest req) throws IOException {
	}

	@Override
	public void call(PxpRequest req, AsyncReturn<Object> asyncRet) throws IOException {
		receivers.offer(asyncRet);
	}
}
