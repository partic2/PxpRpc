package pursuer.pxprpc;

import java.io.IOException;
import java.lang.reflect.Method;

public class GetMethodOfNameFunc extends AbstractCallable{

	@Override
	public void call(AsyncReturn<Object> asyncRet) throws IOException {
		Class<?> classObj=(Class<?>)readNext(8);
		String methodName=readNextString();
		Method found=null;
		for(Method method:classObj.getMethods()) {
			if(method.getName().equals(methodName)) {
				found=method;
				break;
			}
		}
		asyncRet.result(found);
	}

}
