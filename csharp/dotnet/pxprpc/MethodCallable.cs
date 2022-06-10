using System;
using System.Collections.Generic;
using System.Reflection;
using System.Text;

namespace pxprpc
{
    public class MethodCallable : AbstractCallable
    {


        public MethodInfo method;

        public int[] argList;
        public int returnType;

        protected int firstInputParamIndex = 0;

        public MethodCallable(MethodInfo method)
        {
            this.method = method;
            var paras = method.GetParameters();
            argList = new int[paras.Length];
            if (paras.Length > 0 && paras[0].ParameterType == typeof(Action<Object>))
            {
                firstInputParamIndex = 1;
            }
            for (int i = firstInputParamIndex; i < paras.Length; i++)
            {
                var pc = paras[i];
                argList[i] = csTypeToSwitchId(pc.ParameterType);
            }
            returnType = csTypeToSwitchId(method.ReturnType);
        }


        public override void readParameter(PxpRequest req)
        {
            ServerContext c = req.context;
            int thisObj = c.readInt32();
            Object[] args = new Object[argList.Length];
            if (firstInputParamIndex >= 1)
            {
                args[0] = null;
            }
            for (int i = firstInputParamIndex; i < argList.Length; i++)
            {
                args[i] = readNext(c, argList[i]);
            }
            req.parameter = new Object[] { thisObj, args };
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
            result = method.Invoke(ctx.refSlots[(int)((Object[])req.parameter)[0]], args);
            if (firstInputParamIndex == 0)
            {
                asyncRet(result);
            }

        }

    }
}
