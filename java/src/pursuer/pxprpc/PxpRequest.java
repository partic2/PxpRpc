package pursuer.pxprpc;

public class PxpRequest {
	public ServerContext context;
	public byte[] session;
	public int opcode;
	public int destAddr;
	public int srcAddr;
	public Object parameter;
	public Object result;
	public PxpCallable callable;
	public boolean pending=false;
}
