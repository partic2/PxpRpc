package pursuer.pxprpc;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class ServerContext implements Closeable{
	public InputStream in;
	public OutputStream out;
	public boolean running=true;
	public Map<Integer,Object> refSlots=new HashMap<Integer,Object>();
	public Map<String,Object> funcMap=new HashMap<String,Object>();
	public Executor executor=Executors.newCachedThreadPool();
	protected BuiltInFuncList builtIn;
	public void init(InputStream in,OutputStream out) {
		this.in=in;
		this.out=out;
		builtIn=new BuiltInFuncList();
		funcMap.put("builtin", builtIn);
	}
	protected List<PxpRequest> requests=new ArrayList<PxpRequest>();
	
	public void serve() throws IOException {
		while(running) {
			PxpRequest r=new PxpRequest();
			r.context=this;
			byte[] session=new byte[4];
			this.in.read(session);
			r.session=session;
			r.opcode=session[0];
			switch(r.opcode) {
			case 1:
				r.destAddr=readInt32();
				int len=readInt32();
				byte[] buf=new byte[len];
				in.read(buf);
				r.parameter=buf;
				push(r);
				break;
			case 2:
				r.srcAddr=readInt32();
				pull(r);
				break;
			case 3:
				r.destAddr=readInt32();
				r.srcAddr=readInt32();
				assign(r);
				break;
			case 4:
				r.destAddr=readInt32();
				unlink(r);
				break;
			case 5:
				r.destAddr=readInt32();
				r.srcAddr=readInt32();
				r.callable=(PxpCallable) refSlots.get(r.srcAddr);
				r.callable.readParameter(r);
				call(r);
				break;
			case 6:
				r.destAddr=readInt32();
				r.srcAddr=readInt32();
				getFunc(r);
				break;
			case 7:
				close();
				running=false;
				break;
			}
		}
		running=false;
	}
	
	public void push(final PxpRequest r) throws IOException {
		refSlots.put(r.destAddr,r.parameter);
		writeLock().lock();
		this.out.write(r.session);
		writeLock().unlock();
		out.flush();
	}
	public void pull(final PxpRequest r) throws IOException {
		Object o=refSlots.get(r.srcAddr);
		writeLock().lock();
		this.out.write(r.session);
		if(o instanceof byte[]) {
			byte[] b=(byte[]) o;
			writeInt32(b.length);
			out.write(b);
		}else if(o instanceof String){
			byte[] b=((String)o).getBytes("utf-8");
			writeInt32(b.length);
			out.write(b);
		}else {
			writeInt32(-1);
		}
		writeLock().unlock();
		out.flush();
	}
	public void assign(final PxpRequest r) throws IOException {
		refSlots.put(r.destAddr, refSlots.get(r.srcAddr));
		writeLock().lock();
		this.out.write(r.session);
		writeLock().unlock();
		out.flush();
	}
	public void unlink(final PxpRequest r) throws IOException {
		refSlots.remove(r.destAddr);
		writeLock().lock();
		this.out.write(r.session);
		writeLock().unlock();
		out.flush();
	}
	public void call(final PxpRequest r) throws IOException {
		r.pending=true;
		r.callable.call(r,new AsyncReturn<Object>() {
			@Override
			public void result(Object result) {
				try {
					r.result=result;
					refSlots.put(r.destAddr, result);
					r.pending=false;
					writeLock().lock();
					ServerContext.this.out.write(r.session);
					r.callable.writeResult(r);
					writeLock().unlock();
					out.flush();
				} catch (IOException e) {
				}
			}
		});
	}
	public void getFunc(final PxpRequest r) throws IOException {
		String name=getStringAt(r.srcAddr);
		int namespaceDelim=name.indexOf(".");
		String namespace=name.substring(0,namespaceDelim);
		String func=name.substring(namespaceDelim+1);
		Object obj=funcMap.get(namespace);
		Method found=builtIn.getMethod(obj, func);
		writeLock().lock();
		if(found==null) {
			this.out.write(r.session);
			writeInt32(0);
		}else {
			refSlots.put(r.destAddr, new BoundMethodCallable(found, obj));
			this.out.write(r.session);
			writeInt32(r.destAddr);
		}
		writeLock().unlock();
		out.flush();
	}
	
	//TODO: try to implement feature that same session executed in sequence.
	//WIP
	public void requestHandler()throws IOException {
		ArrayList<byte[]> pendingSession=new ArrayList<byte[]>();
		int size=requests.size();
		int i1;
		while(running) {
			PxpRequest r=null;
			synchronized (requests) {
				for(i1=0;i1<size;i1++) {
					PxpRequest elem = requests.get(i1);
					if(elem.pending) {
						pendingSession.add(elem.session);
					}else {
						boolean pending=false;
						for(byte[] elem2:pendingSession) {
							if(elem2[1]==elem.session[1]&&elem2[2]==elem.session[2]&&elem2[3]==elem.session[3]) {
								pending=true;
								break;
							}
						}
						if(!pending) {
							break;
						}
					}
				}
				if(i1<size) {
					r=requests.get(i1);
				}
			}
			if(r!=null) {
				switch(r.opcode) {
				case 1:
					push(r);
					break;
				case 2:
					pull(r);
					break;
				case 3:
					assign(r);
					break;
				case 4:
					unlink(r);
					break;
				case 5:
					//TODO: We need custom 'call' to get return status of callable.
					break;
				case 6:
					getFunc(r);
					break;
				case 7:
					close();
					running=false;
					break;
				}
				if(r.opcode!=5) {
					synchronized (requests) {
						requests.remove(i1);
					}
				}
			}
		}
		
		
	}
	
	private ReentrantLock writeLock=new ReentrantLock();
	public Lock writeLock() {
		return writeLock;
	}
	

	public int readInt32() throws IOException {
		return Utils.readInt32(in);
	}
	public long readInt64() throws IOException {
		return Utils.readInt64(in);
	}
	public float readFloat32() throws IOException {
		return Utils.readFloat32(in);
	}
	public double readFloat64() throws IOException {
		return Utils.readFloat64(in);
	}
	public byte[] readNextRaw() throws IOException {
		int addr=readInt32();
		return (byte[])refSlots.get(addr);
	}
	public static final Charset charset=Charset.forName("utf-8");
	
	public String readNextString() throws IOException {
		int addr=readInt32();
		return getStringAt(addr);
	}
	
	public String getStringAt(int addr) {
		Object o=refSlots.get(addr);
		if(o instanceof byte[]) {
			return new String((byte[])o,charset);
		}else {
			return o.toString();
		}
	}
	
	public void writeInt32(int d) throws IOException {
		Utils.writeInt32(out, d);
	}
	public void writeInt64(long d) throws IOException {
		Utils.writeInt64(out, d);
	}
	public void writeFloat32(float d) throws IOException {
		Utils.writeFloat32(out, d);
	}
	public void writeFloat64(double d) throws IOException {
		Utils.writeFloat64(out, d);
	}
	private void closeQuietly(Closeable c) {
		try {
			c.close();
		}catch(Exception e) {};
	}
	@Override
	public void close() throws IOException {
		running=false;
		closeQuietly(in);
		closeQuietly(out);
		refSlots.clear();
	}
}
