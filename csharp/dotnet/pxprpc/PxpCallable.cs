using System;
using System.Collections.Generic;
using System.Text;

namespace pxprpc
{
	public interface PxpCallable
	{
		void readParameter(PxpRequest req);
        void call(PxpRequest req, Action<Object> asyncRet);
		void writeResult(PxpRequest req);
	}
}