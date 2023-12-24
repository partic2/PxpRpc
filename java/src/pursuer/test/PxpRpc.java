package pursuer.test;

import java.io.Closeable;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;
import java.nio.channels.SocketChannel;
import java.util.Arrays;
import java.util.Timer;
import java.util.TimerTask;

import pursuer.pxprpc.*;
import pursuer.pxprpc_ex.TCPBackend;

public class PxpRpc {
	
	//Just for test, will ignore session check.

	//Rpc server handler demo.
	public static class Handler1 {
		public static Timer tm=new Timer(true);
		public int getInt2345() {
			return 2345;
		}
		public Object get1234() {
			return "1234";
		}
		public void printString(Object s) {
			if(s instanceof byte[]){
				System.out.println(new String((byte[])s,ServerContext.charset));
			}else{
				System.out.println(s);
			}
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
		public void printArg(int a,long b,float c,double d,byte[] e) {
			System.out.println(a+","+b+","+c+","+d+","+
					Arrays.toString(e));
		}
		public void printArg2(byte[] bb){
			Serializer2 ser = new Serializer2().prepareUnserializing(ByteBuffer.wrap(bb));
			int a=ser.getInt();
			long b=ser.getLong();
			float c=ser.getFloat();
			double d=ser.getDouble();
			String e=ser.getString();
			byte[] f=ser.getBytes();
			System.out.println(a+","+b+","+c+","+d+","+e+","+ Arrays.toString(f));
		}
		public void tableTest(byte[] bb){
			TableSerializer ser=new TableSerializer().load(ByteBuffer.wrap(bb));
			for(String e:ser.getHeaderName()){
				System.out.print(e+"\t");
			}
			System.out.println();
			int rcnt=ser.getRowCount();
			for(int i=0;i<rcnt;i++){
				Object[] row=ser.getRow(i);
				for(int i2=0;i2<row.length;i2++){
					System.out.print(row[i2].toString()+"\t");
				}
				System.out.println();
			}
		}
		public TickEvent onTick() {
			TickEvent te = new TickEvent();
			te.start();
			return te;
		}
		//as Parameter class is support since 1.8, We have to use return type to identify the signature even for async callable.
		public Object waitOneTick(final AsyncReturn<Object> asyncRet) {
			tm.schedule(new TimerTask() {
				@Override
				public void run() {
					asyncRet.result("one tick done");
				}
			}, 1000);
			return null;
		}
		public void throwError() throws IOException {
			throw new IOException("dummy exception");
		}
	}
	public static class TickEvent extends EventDispatcher{
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
						e.printStackTrace();
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
			
			SocketChannel soc=SocketChannel.open();
			soc.configureBlocking(true);
			soc.connect(new InetSocketAddress("localhost",listenPort));
			
			final ClientContext client = new ClientContext();
			client.init(soc);
			
			
			System.out.println("##connected server info:\n"+client.getInfo());
			
			
			// set *12 = getFunc test1.getInt2345
			client.getFunc(12, "test1.getInt2345");
			System.out.println("expect print 2345");
			// set *11 = call *12
			System.out.println(client.callFunc(11, 12, new Object[0]).getInt());
			// set *11 = getFunc test1.print5678
			client.getFunc(11,"test1.print5678");
			System.out.println("expect print 5678");
			// set *12 = call *11
			client.callFunc(12, 11, new Object[0]);
			//set *11 = getFunc test1.get1234
			client.getFunc(11, "test1.get1234");
			// set *12 = call *11
			client.callFunc(12,11,new Object[0]);
			//set *13 = getFunc test1.printString
			client.getFunc(13, "test1.printString");
			//set *14 = call *13 (*12)  
			//currently, 12 slot store the value return by test1.get1234
			System.out.println("expect print 1234");
			client.callFunc(14,13,new Object[] {12});
			//set *12="pxprpc"
			client.push(12, "pxprpc".getBytes("utf-8"));
			//should print pxprpc
			System.out.println("expect print pxprpc\\n");
			client.callFunc(14,13,new Object[] {12});
			
			//set *11 = getFunc test1.printArg
			client.getFunc(11, "test1.printArg");
			//call *11(111,2222222,3.14,3.1415,[1,2,3,4])
			System.out.println("expect 111,2222222,3.14,3.1415,[1,2,3,4]");
			client.callFunc(13, 11, new Object[] {111,2222222l,3.14f,3.1415d,new byte[]{1,2,3,4}});

			//set *11 = getFunc test1.printArg2
			client.getFunc(11, "test1.printArg2");
			System.out.println("expect 111,2222222,3.14,3.1415,abcd,[1,2,3,4]");
			client.callFunc(13, 11, new Object[] {Utils.toBytes(new Serializer2().prepareSerializing(64)
					.putInt(111).putLong(2222222l).putFloat(3.14f).putDouble(3.1415d)
					.putString("abcd").putBytes(new byte[]{1,2,3,4},0,4).build())});

			//serilizer test2
			client.getFunc(11, "test1.tableTest");
			System.out.println("expect print a table");
			client.callFunc(13, 11, new Object[] {Utils.toBytes(new TableSerializer().setHeader("sil",new String[]{"name","isDir","size"})
					.addRow(new Object[]{"1.txt",0,45l}).addRow(new Object[]{"docs",1,1122334455667788l}).build())});


			System.out.println("sleep 1 tick, and expect print 'one tick done' ");
			//set *11=getFunc test1.waitOneTick
			client.getFunc(11, "test1.waitOneTick");
			int tickDoneRet=client.callFunc(12,11,new Object[] {}).getInt();
			System.out.println(new String(client.pull(tickDoneRet),"utf-8"));
			
			//set *11=getFunc test1.onTick
			client.getFunc(11, "test1.onTick");
			//set *12=test1.onTick()
			client.callFunc(12,11,new Object[0]);
			//set *13=(*12).next()
			System.out.println(client.callFunc(13,12,new Object[0]).getString());
			//set *13=(*12).next()
			System.out.println(client.callFunc(13,12,new Object[0]).getString());
			//set *13=(*12).next()
			System.out.println(client.callFunc(13,12,new Object[0]).getString());
			
			System.out.println("check exception test,expect get dummy exception");
			//set *11=getFunc test1.throwError
			client.getFunc(11, "test1.throwError");
			//*12=(*11)()
			try{
				client.callFunc(12, 11, new Object[0]);
			}catch(Exception e){
				e.printStackTrace(System.out);
			}
			
			
			//set *11 = getFunc test1.printString
			client.getFunc(11,"test1.printWhenFree");
			//set *12 = call *11
			client.callFunc(12, 11, new Object[0]);
			//push to 12, so *11 should be free if server support.
			System.out.println("expect print 'free by server gc' if server support");
			client.push(12, new byte[0]);

			client.sequence();
			System.out.println("expect print 5678 in 1 second later.");
			client.getFunc(11,"test1.waitOneTick");
			client.getFunc(12,"test1.print5678");
			client.seqCallFuncReq(13,11,new Object[]{});
			client.seqCallFuncReq(13,12,new Object[]{});
			client.seqCallFuncResp();
			client.seqCallFuncResp();

			//should be free when connection close, if server support. 
			client.callFunc(12, 11, new Object[0]);
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
		public ByteChannel chan;
		
		public void init(ByteChannel chan) {
			this.chan=chan;
		}
		public void assert2(boolean b) {
			if(!b) {
				throw new RuntimeException("assert error");
			}
		}
		public int session=0x78593<<8;
		public int sequenceSession=0x78597<<8;
		
		public void push(int addr,byte[] data) throws IOException {
			int op=session|0x1;
			Utils.writeInt32(chan,op);
			Utils.writeInt32(chan,addr);
			Utils.writeInt32(chan, data.length);
			Utils.writef(chan,ByteBuffer.wrap(data));
			int op2=Utils.readInt32(chan);
			assert2(op2==op);
		}
		public byte[] pull(int addr) throws IOException {
			int op=session|0x2;
			Utils.writeInt32(chan,op);
			Utils.writeInt32(chan,addr);
			int rs=Utils.readInt32(chan);
			assert2(rs==op);
			int len=Utils.readInt32(chan);
			byte[] r=new byte[len];
			Utils.readf(chan,ByteBuffer.wrap(r));
			return r;
		}
		public Serializer2 callFunc(int assignAddr, int addr, Object[] params) throws IOException {
			int op=sequenceSession|0x5;
			Utils.writeInt32(chan,op);
			Utils.writeInt32(chan,assignAddr);
			Utils.writeInt32(chan,addr);
			Serializer2 ser=new Serializer2().prepareSerializing(32);
			for(Object p : params) {
				if(p.getClass().equals(Integer.class)) {
					ser.putInt((Integer) p);
				}else if(p.getClass().equals(Long.class)) {
					ser.putLong((Long) p);
				}else if(p.getClass().equals(Float.class)) {
					ser.putFloat((Float) p);
				}else if(p.getClass().equals(Double.class)) {
					ser.putDouble((Double)p);
				} else if (p.getClass().equals(byte[].class)) {
					byte[] b2= (byte[]) p;
					ser.putBytes(b2,0,b2.length);
				}else if(p.getClass().equals(String.class)){
					ser.putString((String)p);
				}else {
					throw new UnsupportedOperationException();
				}
			}
			ByteBuffer buf = ser.build();
			Utils.writeInt32(chan,buf.remaining());
			Utils.writef(chan,buf);
			assert2(Utils.readInt32(chan)==op);
			int len=Utils.readInt32(chan);
			buf=ByteBuffer.allocate(len&0x7fffffff);
			Utils.readf(chan,buf);
			ser = new Serializer2().prepareUnserializing(buf);
			if((len&0x80000000)!=0){
				throw new RuntimeException(ser.getString());
			}else{
				return ser;
			}
		}
		public int getFunc(int assignAddr,String name) throws IOException {
			int op=session|0x6;
			push(1, name.getBytes());
			Utils.writeInt32(chan,op);
			Utils.writeInt32(chan,assignAddr);
			Utils.writeInt32(chan, 1);
			assert2(Utils.readInt32(chan)==op);
			return Utils.readInt32(chan);
		}
		public String getInfo()throws IOException{
			int op=session|0x8;
			Utils.writeInt32(chan,op);
			assert2(Utils.readInt32(chan)==op);
			int len=Utils.readInt32(chan);
			byte[] r=new byte[len];
			Utils.readf(chan,ByteBuffer.wrap(r));
			return new String(r,"utf-8");
		}
		public void sequence() throws IOException{
			int op=session|0x9;
			Utils.writeInt32(chan,op);
			Utils.writeInt32(chan,sequenceSession|24);
			int op2=Utils.readInt32(chan);
			assert2(op2==op);
		}
		public void buffer() throws IOException {
			int op=session|0xa;
			Utils.writeInt32(chan,op);
		}
		public void seqCallFuncReq(int assignAddr,int addr,Object[] params) throws IOException {
			int op=sequenceSession|0x5;
			Utils.writeInt32(chan,op);
			Utils.writeInt32(chan,assignAddr);
			Utils.writeInt32(chan,addr);
			Serializer2 ser=new Serializer2().prepareSerializing(32);
			for(Object p : params) {
				if(p.getClass().equals(Integer.class)) {
					ser.putInt((Integer) p);
				}else if(p.getClass().equals(Long.class)) {
					ser.putLong((Long) p);
				}else if(p.getClass().equals(Float.class)) {
					ser.putFloat((Float) p);
				}else if(p.getClass().equals(Double.class)) {
					ser.putDouble((Double)p);
				} else if (p.getClass().equals(byte[].class)) {
					byte[] b2= (byte[]) p;
					ser.putBytes(b2,0,b2.length);
				}else if(p.getClass().equals(String.class)){
					ser.putString((String)p);
				}else {
					throw new UnsupportedOperationException();
				}
			}
			ByteBuffer buf = ser.build();
			Utils.writeInt32(chan,buf.remaining());
			Utils.writef(chan,buf);
		}
		public Serializer2 seqCallFuncResp() throws IOException {
			int op=sequenceSession|0x5;
			assert2(Utils.readInt32(chan)==op);
			int len=Utils.readInt32(chan);
			ByteBuffer buf=ByteBuffer.allocate(len&0x7fffffff);
			Utils.readf(chan,buf);
			Serializer2 ser = new Serializer2().prepareUnserializing(buf);
			if((len&0x80000000)!=0){
				throw new RuntimeException(ser.getString());
			}else{
				return ser;
			}
		}

	}
}
