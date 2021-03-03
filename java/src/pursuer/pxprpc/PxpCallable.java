package pursuer.pxprpc;

import java.io.IOException;

public interface PxpCallable {
	void context(ServerContext ctx);
	void call(AsyncReturn<Object> asyncRet) throws IOException;
	void writeResult(Object result,int resultAddr) throws IOException;
}
