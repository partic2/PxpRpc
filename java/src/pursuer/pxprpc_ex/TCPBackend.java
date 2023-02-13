package pursuer.pxprpc_ex;

import java.io.Closeable;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.channels.AsynchronousCloseException;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import pursuer.pxprpc.ServerContext;

public class TCPBackend implements Closeable{
	public TCPBackend() {
	}
	public InetSocketAddress bindAddr;
	public Map<String,Object> funcMap=new HashMap<String,Object>();
	protected ServerSocketChannel ss;
	public Set<WorkThread> runningContext=new HashSet<WorkThread>();
	public static class WorkThread implements Runnable{
		public SocketChannel s;
		public ServerContext sc;
		public TCPBackend attached;
		public WorkThread(SocketChannel s,ServerContext sc) {
			this.s=s;
			this.sc=sc;
		}
		@Override
		public void run() {
			try {
				sc.init(s);
				s.configureBlocking(true);
				sc.serve();
			} catch (Exception e) {
				//catch all exception to avoid breaking Application unexpectedly.
				if(!(e instanceof IOException)){
					e.printStackTrace();
				}
			}finally{
				try {sc.close();} catch (IOException e) {}
				if(attached!=null) {
					attached.runningContext.remove(this);
				}
			}
			
		}
	}
	protected void execute(Runnable r) {
		Thread t=new Thread(r);
		t.setDaemon(true);
		t.start();
	}
	public void listenAndServe() throws IOException {
		try {
			ss=ServerSocketChannel.open();
			//chan.bind since 1.7, so we choose socket.bind.
			ss.socket().bind(bindAddr);
			while(ss.isOpen()) {
				SocketChannel soc = ss.accept();
				ServerContext sc = new ServerContext();
				sc.funcMap=this.funcMap;
				WorkThread wt = new WorkThread(soc, sc);
				wt.attached=this;
				runningContext.add(wt);
				execute(wt);
			}
		}catch(AsynchronousCloseException c) {}
	}
	
	@Override
	public void close() throws IOException {
		//Copy runningContext to avoid modifing to iterating
		ArrayList<WorkThread> snapshot = new ArrayList<WorkThread>();
		snapshot.addAll(runningContext);
		for(WorkThread wt:snapshot) {
			if(wt.sc.running) {
				wt.sc.close();
			}
		}
		ss.close();
	}
}
