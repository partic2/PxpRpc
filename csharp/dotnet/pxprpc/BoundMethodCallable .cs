using System;
using System.Collections.Generic;
using System.Reflection;
using System.Text;

namespace pxprpc
{
    public class BoundMethodCallable : MethodCallable
    {
        protected Object boundObj;
        public BoundMethodCallable(MethodInfo method, Object boundObj):base(method)
        {
            this.boundObj = boundObj;
        }
        public override void readParameter(PxpRequest req)
        {
            ServerContext c = req.context;
            Object[] args = new Object[argList.Length];
            if (firstInputParamIndex >= 1)
            {
                args[0] = null;
            }
            for (int i = firstInputParamIndex; i < argList.Length; i++)
            {
                args[i] = readNext(c, argList[i]);
            }
            req.parameter = new Object[] { null, args };
        }
        public override void call(PxpRequest req, Action<Object> asyncRet)
        {
            ServerContext ctx = req.context;

            Object result = null;
            Object[] args = (Object[])((Object[])req.parameter)[1];
            if (firstInputParamIndex >= 1)
            {
                args[0] = asyncRet;
            }
            try
            {
                result = method.Invoke(this.boundObj, args);
                if (firstInputParamIndex == 0)
                {
                    asyncRet(result);
                }
            }
            catch (Exception ex)
            {
                asyncRet(ex.InnerException);
            }

            
        }
    }
}
