package pxprpc.base;

import java.io.IOException;

public interface PxpCallable {
	void call(PxpRequest req) throws IOException;
}

