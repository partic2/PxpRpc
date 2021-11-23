package pursuer.pxprpc;

import java.io.IOException;

public interface PxpCallable {
	void readParameter(PxpRequest req) throws IOException ;
	void call(PxpRequest req,AsyncReturn<Object> asyncRet) throws IOException;
	void writeResult(PxpRequest req) throws IOException;
}
