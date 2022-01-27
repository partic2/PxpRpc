package pursuer.test;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.Timer;
import java.util.TimerTask;

import pursuer.pxprpc.AsyncReturn;
import pursuer.pxprpc.EventDispatcher;
import pursuer.pxprpc.Utils;
import pursuer.pxprpc_ex.TCPBackend;

public class PxpRpc {
	
	//Just for test, will ignore session check.
	

	//Rpc server handler demo.
	public static class Handler1 {
		public String get1234() {
			return "1234";
		}
		public void printString(String s) {
			System.out.println(s);
		}
		public void print5678() {
			System.out.println("5678");
		}
		public Closeable printWhenFree() {
			return new Closeable() {
				@Override
				public void close() throws IOException {
					System.out.println("free by server gc");
				}
			};
		}
		public TickEvent onTick() {
			TickEvent te = new TickEvent();
			te.start();
			return te;
		}
		public void waitOneTick(final AsyncReturn<Object> asyncRet) {
			Timer tm=new Timer();
			tm.schedule(new TimerTask() {
				@Override
				public void run() {
					asyncRet.result("one tick done");
				}
			}, 1000);
		}
	}
	public static class TickEvent extends EventDispatcher{
		Timer tm=new Timer();
		public TickEvent() {
		}
		public void start() {
			tm.schedule(new TimerTask() {
				@Override
				public void run() {
					TickEvent.this.fireEvent("tick");
				}
			}, 1000,1000);
		}
		public void stop() {
			tm.cancel();
		}
	}

	public static void main(String[] args) {

		try {
			final TCPBackend pxptcp = new TCPBackend();
			int listenPort=2064;
			pxptcp.funcMap.put("test1", new Handler1());
			pxptcp.bindAddr=new InetSocketAddress(listenPort);
			Thread th=new Thread(new Runnable() {
				@Override
				public void run() {
					try {
						pxptcp.listenAndServe();
					} catch (IOException e) {
						if(!e.getMessage().contains("Interrupted function call")) {
							e.printStackTrace();
						}
					}
					System.out.println("server stoped");
				}
			});
			th.setDaemon(true);
			th.start();
			
			System.out.println("waiting for server start");
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
			}
			
			Socket soc=new Socket();
			soc.connect(new InetSocketAddress("localhost",listenPort));
			
			ClientContext client = new ClientContext();
			client.init(soc.getInputStream(),soc.getOutputStream());
			
			System.out.println("##connected server info:\n"+client.getInfo());
			// set *11 = getFunc test1.print5678
			client.getFunc(11,"test1.print5678");
			System.out.println("expect print 5678");
			// set *12 = call *11
			client.callIntFunc(12, 11, new Object[0]);
			//set *11 = getFunc test1.get1234
			client.getFunc(11, "test1.get1234");
			// set *12 = call *11
			client.callIntFunc(12,11,new Object[0]);
			//set *13 = getFunc test1.printString
			client.getFunc(13, "test1.printString");
			//set *14 = call *13 (*12)  
			//currently, 12 slot store the value return by test1.get1234
			System.out.println("expect print 1234");
			client.callIntFunc(14,13,new Object[] {12});
			//set *12="pxprpc"
			client.push(12, "pxprpc".getBytes("utf-8"));
			//should print pxprpc
			System.out.println("expect print pxprpc");
			client.callIntFunc(14,13,new Object[] {12});
			
			System.out.println("sleep 1 tick");
			//set *11=getFunc test1.waitOneTick
			client.getFunc(11, "test1.waitOneTick");
			//set *12=call *11
			client.callIntFunc(12, 11, new Object[0]);
			System.out.println(new String(client.pull(12),"utf-8"));
			
			//set *11=getFunc test1.onTick
			client.getFunc(11, "test1.onTick");
			//set *12=test1.onTick()
			System.out.println(client.callIntFunc(12,11,new Object[0]));
			//set *13=(*12).next()
			client.callIntFunc(13,12,new Object[0]);
			System.out.println(new String(client.pull(13),"utf-8"));
			//set *13=(*12).next()
			client.callIntFunc(13,12,new Object[0]);
			System.out.println(new String(client.pull(13),"utf-8"));
			//set *13=(*12).next()
			client.callIntFunc(13,12,new Object[0]);
			System.out.println(new String(client.pull(13),"utf-8"));
			
			//set *11 = getFunc test1.printString
			client.getFunc(11,"test1.printWhenFree");
			//set *12 = call *11
			client.callIntFunc(12, 11, new Object[0]);
			//push to 12, so *11 should be free if server support.
			System.out.println("expect print 'free by server gc' if server support");
			client.push(12, new byte[0]);
			
			//should be free if server support when connection close.
			client.callIntFunc(12, 11, new Object[0]);
			pxptcp.close();
			System.out.println("close server...");
			
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
			}
			
		} catch (IOException e) {
			e.printStackTrace();
		}

	}
	
	public static class ClientContext {
		public InputStream in;
		public OutputStream out;
		
		public void init(InputStream in,OutputStream out) {
			this.in=in;
			this.out=out;
		}
		public void assert2(boolean b) {
			if(!b) {
				throw new RuntimeException("assert error");
			}
		}
		public int session=0x78593<<8;
		
		public void push(int addr,byte[] data) throws IOException {
			int op=session|0x1;
			Utils.writeInt32(out,op);
			Utils.writeInt32(out,addr);
			Utils.writeInt32(out, data.length);
			out.write(data);
			out.flush();
			int op2=Utils.readInt32(in);
			assert2(op2==op);
		}
		public byte[] pull(int addr) throws IOException {
			int op=session|0x2;
			Utils.writeInt32(out,op);
			Utils.writeInt32(out,addr);
			assert2(Utils.readInt32(in)==op);
			out.flush();
			int len=Utils.readInt32(in);
			byte[] r=new byte[len];
			in.read(r);
			return r;
		}
		public int callIntFunc(int assignAddr,int addr,Object[] params) throws IOException {
			int op=session|0x5;
			Utils.writeInt32(out,op);
			Utils.writeInt32(out,assignAddr);
			Utils.writeInt32(out,addr);
			for(Object p : params) {
				//TODO: Other data type.
				if(p.getClass().equals(Integer.class)) {
					Utils.writeInt32(out,(Integer) p);
				}else {
					throw new UnsupportedOperationException();
				}
			}
			out.flush();
			assert2(Utils.readInt32(in)==op);
			return Utils.readInt32(in);
		}
		public int getFunc(int assignAddr,String name) throws IOException {
			int op=session|0x6;
			push(1, name.getBytes());
			Utils.writeInt32(out,op);
			Utils.writeInt32(out,assignAddr);
			Utils.writeInt32(out, 1);
			out.flush();
			assert2(Utils.readInt32(in)==op);
			return Utils.readInt32(in);
		}
		public String getInfo()throws IOException{
			int op=session|0x8;
			Utils.writeInt32(out,op);
			assert2(Utils.readInt32(in)==op);
			out.flush();
			int len=Utils.readInt32(in);
			byte[] r=new byte[len];
			in.read(r);
			return new String(r,"utf-8");
		}
	}
}
