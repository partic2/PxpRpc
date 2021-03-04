package pursuer.pxprpc;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class ServerContext implements Closeable{
	public InputStream in;
	public OutputStream out;
	public boolean running=true;
	public Map<Integer,Object> refSlots=new HashMap<Integer,Object>();
	public Map<String,Object> funcMap=new HashMap<String,Object>();
	
	private BuiltInFuncList builtIn;
	public void init(InputStream in,OutputStream out) {
		this.in=in;
		this.out=out;
		builtIn=new BuiltInFuncList();
		funcMap.put("builtin", builtIn);
	}
	public void serve() throws IOException {
		while(running) {
			int opcode=in.read();
			switch(opcode) {
			case 1:
				push();
				break;
			case 2:
				pull();
				break;
			case 3:
				assign();
				break;
			case 4:
				unlink();
				break;
			case 5:
				call();
				break;
			case 6:
				getFunc();
				break;
			case 7:
				close();
				running=false;
				break;
			}
		}
		running=false;
	}
	
	public void push() throws IOException {
		int session=readInt32();
		int addr=readInt32();
		int len=readInt32();
		byte[] buf=new byte[len];
		in.read(buf);
		refSlots.put(addr,buf);
		writeLock().lock();
		writeInt32(session);
		writeLock().unlock();
		out.flush();
	}
	public void pull() throws IOException {
		int session=readInt32();
		int addr=readInt32();
		Object o=refSlots.get(addr);
		writeLock().lock();
		writeInt32(session);
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
	public void assign() throws IOException {
		int session=readInt32();
		int addr=readInt32();
		int srcAddr=readInt32();
		refSlots.put(addr, refSlots.get(srcAddr));
		writeLock().lock();
		writeInt32(session);
		writeLock().unlock();
	}
	public void unlink() throws IOException {
		int session = readInt32();
		int addr=readInt32();
		refSlots.remove(addr);
		writeLock().lock();
		writeInt32(session);
		writeLock().unlock();
		out.flush();
	}
	public void call() throws IOException {
		final int session=readInt32();
		final int retAddr=readInt32();
		int funcAddr=readInt32();
		PxpCallable callable=(PxpCallable) refSlots.get(funcAddr);
		callable.context(this);
		callable.call(new AsyncReturn<Object>() {
			@Override
			public void result(Object result) {
				try {
					refSlots.put(retAddr, result);
					writeLock().lock();
					writeInt32(session);
					callable.writeResult(result, retAddr);
					writeLock().unlock();
					out.flush();
				} catch (IOException e) {
				}
			}
		});
	}
	public void getFunc() throws IOException {
		final int session=readInt32();
		int retAddr=readInt32();
		String name=this.readNextString();
		int namespaceDelim=name.indexOf(".");
		String namespace=name.substring(0,namespaceDelim);
		String func=name.substring(namespaceDelim+1);
		Object obj=funcMap.get(namespace);
		Method found=builtIn.getMethod(obj, func);
		writeLock().lock();
		if(found==null) {
			writeInt32(session);
			writeInt32(0);
		}else {
			refSlots.put(retAddr, new BoundMethodCallable(found, obj));
			writeInt32(session);
			writeInt32(retAddr);
		}
		writeLock().unlock();
		out.flush();
	}
	private ReentrantLock lock=new ReentrantLock();
	public Lock writeLock() {
		return lock;
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
	public String readNextString() throws IOException {
		int addr=readInt32();
		Object o=refSlots.get(addr);
		if(o instanceof byte[]) {
			return new String((byte[])o,"utf-8");
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
