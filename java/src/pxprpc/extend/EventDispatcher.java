package pxprpc.extend;

import pxprpc.base.PxpRequest;

import java.io.Closeable;
import java.io.IOException;
import java.util.LinkedList;
import java.util.Queue;

public class EventDispatcher extends CommonCallable implements Closeable {
	public boolean closed=false;
	public EventDispatcher() {
		this.tResult=new char[]{'o'};
	}
	public EventDispatcher setEventType(Class<?> c){
		this.tResult=new char[]{TypeDeclParser.jtypeToValueInfo(c)};
		return this;
	}
	protected Queue<PxpRequest> receivers = new LinkedList<PxpRequest>();

	public synchronized void fireEvent(Object evt) {
		Queue<PxpRequest> saved = receivers;
		receivers=new LinkedList<PxpRequest>();
		PxpRequest r;
		for(r = saved.poll();r!=null;r=saved.poll()){
			writeResult(r,evt);
			r.done();
		}
	}

	public void clearReceivers() {
		receivers.clear();
	}

	@Override
	public synchronized void call(PxpRequest r) {
		if(this.closed){
			r.rejected=new Error("closed");
			r.done();
		}else {
			receivers.offer(r);
		}
	}

	@Override
	public void close() throws IOException {
		Queue<PxpRequest> saved = receivers;
		receivers=new LinkedList<PxpRequest>();
		PxpRequest r;
		for(r = saved.poll();r!=null;r=saved.poll()){
			r.rejected=new Error("closed");
			r.done();
		}
	}
}
