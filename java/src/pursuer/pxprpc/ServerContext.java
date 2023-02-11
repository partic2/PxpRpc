package pursuer.pxprpc;

import java.io.Closeable;
import java.io.IOException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class ServerContext implements Closeable{
	public static int DefaultRefSlotsCap=256;
	public ByteChannel chan;
	public boolean running=true;
	public Ref[] refSlots=new Ref[DefaultRefSlotsCap];
	public Map<String,Object> funcMap=new HashMap<String,Object>();
	public Executor executor=Executors.newCachedThreadPool();
	protected BuiltInFuncList builtIn;
	public void init(ByteChannel chan) {
		this.chan=chan;
		builtIn=new BuiltInFuncList();
		funcMap.put("builtin", builtIn);
	}
	protected List<PxpRequest> requests=new ArrayList<PxpRequest>();
	
	
	public void serve() throws IOException {
		while(running) {
			PxpRequest r=new PxpRequest();
			r.context=this;
			byte[] session=new byte[4];
			Utils.readf(chan,ByteBuffer.wrap(session));
			r.session=session;
			r.opcode=session[0];
			switch(r.opcode) {
			case 1:
				r.destAddr=readInt32();
				int len=readInt32();
				ByteBuffer buf=ByteBuffer.allocateDirect(len);
				Utils.readf(chan,buf);
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
				r.callable=(PxpCallable) refSlots[r.srcAddr].get();
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
			case 8:
				getInfo(r);
				break;
			}
		}
		running=false;
	}
	
	protected void putRefSlots(int addr,Ref r) {
		if(refSlots[addr]!=null) 
			refSlots[addr].release();
		if(r!=null)
			r.addRef();
		refSlots[addr]=r;
	}
	
	public void push(final PxpRequest r) throws IOException {
		putRefSlots(r.destAddr,new Ref(r.parameter));
		writeLock().lock();
		Utils.writef(this.chan,ByteBuffer.wrap(r.session));
		writeLock().unlock();
	}
	public void pull(final PxpRequest r) throws IOException {
		Object o=null;
		if(refSlots[r.srcAddr]!=null) {
			o=refSlots[r.srcAddr].get();
		}
		writeLock().lock();
		Utils.writef(this.chan,ByteBuffer.wrap(r.session));
		if(o instanceof ByteBuffer) {
			Utils.writeInt32(this.chan, ((Buffer) o).remaining());
			Utils.writef(this.chan,(ByteBuffer) o);
		}if(o instanceof byte[]) {
			byte[] b=(byte[]) o;
			writeInt32(b.length);
			Utils.writef(this.chan,ByteBuffer.wrap(b));
		}else if(o instanceof String){
			byte[] b=((String)o).getBytes("utf-8");
			writeInt32(b.length);
			Utils.writef(this.chan,ByteBuffer.wrap(b));
		}else {
			writeInt32(-1);
		}
		writeLock().unlock();
	}
	public void assign(final PxpRequest r) throws IOException {
		putRefSlots(r.destAddr, this.refSlots[r.srcAddr]);
		writeLock().lock();
		Utils.writef(this.chan,ByteBuffer.wrap(r.session));
		writeLock().unlock();
	}
	public void unlink(final PxpRequest r) throws IOException {
		putRefSlots(r.destAddr, null);
		writeLock().lock();
		Utils.writef(this.chan,ByteBuffer.wrap(r.session));
		writeLock().unlock();
	}
	public void call(final PxpRequest r) throws IOException {
		r.pending=true;
		r.callable.call(r,new AsyncReturn<Object>() {
			@Override
			public void result(Object result) {
				try {
					r.result=result;
					ServerContext.this.putRefSlots(r.destAddr,new Ref(result));
					r.pending=false;
					writeLock().lock();
					Utils.writef(ServerContext.this.chan,ByteBuffer.wrap(r.session));
					r.callable.writeResult(r);
					writeLock().unlock();
				} catch (IOException e) {
				}
			}
		});
	}
	public void getFunc(final PxpRequest r) throws IOException {
		String name=getStringAt(r.srcAddr);
		int namespaceDelim=name.lastIndexOf(".");
		String namespace=name.substring(0,namespaceDelim);
		String func=name.substring(namespaceDelim+1);
		Object obj=funcMap.get(namespace);
		AbstractCallable found=null;
		if(obj!=null){
			found=builtIn.getBoundMethod(obj, func);
		}
		writeLock().lock();
		if(found==null) {
			Utils.writef(this.chan,ByteBuffer.wrap(r.session));
			writeInt32(0);
		}else {
			putRefSlots(r.destAddr, new Ref(found));
			Utils.writef(this.chan,ByteBuffer.wrap(r.session));
			writeInt32(r.destAddr);
		}
		writeLock().unlock();
	}
	public void getInfo(final PxpRequest r)throws IOException{
		writeLock().lock();
		Utils.writef(this.chan,ByteBuffer.wrap(r.session));
		byte[] b=(
		"server name:pxprpc for java\n"+
		"version:1.0\n"+
		"reference slots capacity:"+this.refSlots.length+"\n"
		).getBytes("utf-8");
		writeInt32(b.length);
		Utils.writef(this.chan,ByteBuffer.wrap(b));
		writeLock().unlock();
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
		return Utils.readInt32(chan);
	}
	public long readInt64() throws IOException {
		return Utils.readInt64(chan);
	}
	public float readFloat32() throws IOException {
		return Utils.readFloat32(chan);
	}
	public double readFloat64() throws IOException {
		return Utils.readFloat64(chan);
	}
	public byte[] readNextBytes() throws IOException {
		int addr=readInt32();
		return getBytesAt(addr);
	}
	public byte[] getBytesAt(int addr) {
		Object b = refSlots[addr].get();
		if(b instanceof ByteBuffer) {
			ByteBuffer b2=(ByteBuffer) b;
			Buffer b3=b2;
			b3.mark();
			byte[] ba=new byte[b2.remaining()];
			b2.get(ba);
			b3.reset();
			return ba;
		}else {
			return (byte[])b;
		}
	}
	public static final Charset charset=Charset.forName("utf-8");
	
	public String readNextString() throws IOException {
		int addr=readInt32();
		return getStringAt(addr);
	}
	
	public String getStringAt(int addr) {
		if(refSlots[addr]==null) {
			return "";
		}
		Object o=refSlots[addr].get();
		if(o instanceof ByteBuffer || o instanceof byte[]) {
			return new String(getBytesAt(addr));
		}else {
			return o.toString();
		}
	}
	
	public void writeInt32(int d) throws IOException {
		Utils.writeInt32(chan, d);
	}
	public void writeInt64(long d) throws IOException {
		Utils.writeInt64(chan, d);
	}
	public void writeFloat32(float d) throws IOException {
		Utils.writeFloat32(chan, d);
	}
	public void writeFloat64(double d) throws IOException {
		Utils.writeFloat64(chan, d);
	}
	private void closeQuietly(Closeable c) {
		try {
			c.close();
		}catch(Exception e) {};
	}
	@Override
	public void close() throws IOException {
		running=false;
		closeQuietly(chan);
		for(int i1=0;i1<refSlots.length;i1++) {
			if(refSlots[i1]!=null) {
				refSlots[i1].release();
			}
		}
		System.gc();
	}
}
