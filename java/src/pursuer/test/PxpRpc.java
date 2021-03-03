package pursuer.test;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;

import pursuer.pxprpc.ServerContext;
import pursuer.pxprpc.Utils;

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
			PipedInputStream pipe1in = new PipedInputStream();
			PipedOutputStream pipe1out;
			pipe1out = new PipedOutputStream(pipe1in);
			PipedInputStream pipe2in = new PipedInputStream();
			PipedOutputStream pipe2out = new PipedOutputStream(pipe2in);

			ServerContext ctx = new ServerContext();
			ctx.handlers.put("test1", new Handler1());
			ctx.init(pipe1in, pipe2out);
			Thread th=new Thread(new Runnable() {
				@Override
				public void run() {
					try {
						ctx.serve();
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
			});
			th.setDaemon(true);
			th.start();
			ClientContext client = new ClientContext();
			client.init(pipe2in,pipe1out);
			client.getFunc(1101,"test1.print5678");
			client.callIntFunc(1102, 1101, new Object[0]);
			client.getFunc(1101, "test1.get1234");
			client.callIntFunc(1102,1101,new Object[0]);
			client.getFunc(1103, "test1.printString");
			client.callIntFunc(1104,1103,new Object[] {1102});
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
