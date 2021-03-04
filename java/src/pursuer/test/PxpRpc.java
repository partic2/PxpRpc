package pursuer.test;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

import pursuer.pxprpc.ServerContext;
import pursuer.pxprpc.Utils;
import pursuer.pxprpc_ex.TCPBackend;

public class PxpRpc {
	
	//Just for test, will ignore session check.
	

	
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
	}

	public static void main(String[] args) {

		try {
			TCPBackend pxptcp = new TCPBackend();
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
			
			// set *1101 = getFunc test1.print5678
			client.getFunc(1101,"test1.print5678");
			// set *1102 = call *1101
			client.callIntFunc(1102, 1101, new Object[0]);
			//set *1101 = getFunc test1.get1234
			client.getFunc(1101, "test1.get1234");
			// set *1102 = call *1101
			client.callIntFunc(1102,1101,new Object[0]);
			//set *1103 = getFunc test1.printString
			client.getFunc(1103, "test1.printString");
			//set *1104 = call *1103 (*1102)  
			//currently, 1102 slot store the value return by test1.get1234
			client.callIntFunc(1104,1103,new Object[] {1102});
			
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
		public int session=26769;
		public void push(int addr,byte[] data) throws IOException {
			out.write(1);
			Utils.writeInt32(out,session);
			Utils.writeInt32(out,addr);
			Utils.writeInt32(out, data.length);
			out.write(data);
			out.flush();
			Utils.readInt32(in);
		}
		public byte[] pull(int addr) throws IOException {
			out.write(2);
			Utils.writeInt32(out,session);
			Utils.writeInt32(out,addr);
			Utils.readInt32(in);
			out.flush();
			int len=Utils.readInt32(in);
			byte[] r=new byte[len];
			in.read(r);
			return r;
		}
		public int callIntFunc(int assignAddr,int addr,Object[] params) throws IOException {
			out.write(5);
			Utils.writeInt32(out,session);
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
			Utils.readInt32(in);
			return Utils.readInt32(in);
		}
		public int getFunc(int assignAddr,String name) throws IOException {
			push(1025, name.getBytes());
			out.write(6);
			Utils.writeInt32(out,session);
			Utils.writeInt32(out,assignAddr);
			Utils.writeInt32(out, 1025);
			out.flush();
			Utils.readInt32(in);
			return Utils.readInt32(in);
		}
	}
}
