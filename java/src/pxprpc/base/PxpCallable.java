package pxprpc.base;

import java.io.IOException;

/* Server side callable */
public interface PxpCallable {
	void call(PxpRequest req) throws IOException;
}

