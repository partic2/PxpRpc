package pxprpc.test;

import java.io.Closeable;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.util.*;

import pxprpc.backend.TCPBackend;
import pxprpc.base.Serializer2;
import pxprpc.base.ServerContext;
import pxprpc.base.Utils;
import pxprpc.extend.*;

public class PxpRpc {
	
	//Just for test, will ignore session check.

	//Rpc server handler demo.
	public static class Handler1 {

		public Object get1234(){
			return "1234";
		}

		public void printString(Object s){
			System.out.println(s);
		}
		@MethodTypeDecl("cilfdb->il")
		public Object[] testPrintArg(AsyncReturn<Object[]> r,boolean a,int b,long c,float d,double e,ByteBuffer f){
			System.out.println(""+a+","+b+","+c+","+d+","+e+","+new String(Utils.toBytes(f), ServerContext.charset));
			r.resolve(new Object[]{100,1234567890l});
			return null;
		}

		public void testUnser(ByteBuffer buf){
			Serializer2 ser = new Serializer2().prepareUnserializing(buf);
			System.out.println(","+ser.getInt()+","+ser.getLong()+","+ser.getFloat()+","+ser.getDouble()+","+ser.getString()+","+ser.getString());
		}

		public void testTableUnser(ByteBuffer buf){
			List<Map<String, Object>> maparr = new TableSerializer().load(buf).toMapArray();
			for(Map<String, Object> e:maparr){
				for(Map.Entry<String, Object> kv:e.entrySet()){
					System.out.print(kv.getKey()+":");
					System.out.print(kv.getValue());
					System.out.print("     ");
				}
				System.out.println();
			}
		}
		public Object testNone(AsyncReturn<Object> r,Object para){
			System.out.print("expect null:");
			System.out.println(para);
			r.resolve(null);
			return new Object();
		}

		public TickEvent onTick() {
			TickEvent te = new TickEvent();
			te.start();
			return te;
		}
		public static Timer tm=new Timer();
		//as Parameter class is support since 1.8, We have to use return type to identify the typedecl even for async callable.
		public String wait1Sec(final AsyncReturn<Object> asyncRet) {
			tm.schedule(new TimerTask() {
				@Override
				public void run() {
					asyncRet.resolve("tick");
				}
			}, 1000);
			return null;
		}
		public void raiseError1() throws IOException {
			throw new IOException("dummy exception");
		}
		public Closeable autoCloseable(){
			return new Closeable(){
				@Override
				public void close() throws IOException {
					System.out.println("auto closeable closed");
				}
			};
		}
	}
	public static class TickEvent extends EventDispatcher {
		public TickEvent() {
			setEventType(String.class);
		}
		public void start() {
			Handler1.tm.schedule(new TimerTask() {
				@Override
				public void run() {
					TickEvent.this.fireEvent("tick");
				}
			}, 1000,1000);
		}
	}


	public static class Cfg{
		public int id;
		public long size;
		public String name;
		public boolean isdir;

		@Override
		public String toString() {
			StringBuilder sb = new StringBuilder();
			sb.append("id:");
			sb.append(id);
			sb.append(",size:");
			sb.append(size);
			sb.append(",name:");
			sb.append(name);
			sb.append(",isdir:");
			sb.append(isdir);
			return sb.toString();
		}
	}
	public static void tabserTest() {
		Cfg t1 = new Cfg();
		t1.id=12;
		t1.isdir=false;
		t1.name="myfile.txt";
		t1.size=1231l;
		ArrayList<Cfg> t2 = new ArrayList<Cfg>();
		t2.add(t1);
		ByteBuffer sered = new TableSerializer().fromTypedObjectArray(t2).build();
		System.out.println(new TableSerializer().load(sered).toTypedObjectArray(Cfg.class));
	}

	public static void main(String[] args) {
		tabserTest();
		final TCPBackend pxptcp = new TCPBackend();
		int listenPort=1064;
		DefaultFuncMap.registered.put("test1", new Handler1());
		pxptcp.bindAddr=new InetSocketAddress(listenPort);
		Thread th=new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					System.out.println("ready...");
					pxptcp.listenAndServe();
				} catch (IOException e) {
					e.printStackTrace();
					if(!e.getMessage().contains("Interrupted function call")) {
						e.printStackTrace();
					}
				}
				System.out.println("server stoped");
			}
		});
		th.start();
	}
}
