using System;
using System.Collections.Generic;
using System.Reflection;
using System.Text;

namespace pxprpc
{
    public class MethodCallable : PxpCallable
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


        public virtual void readParameter(PxpRequest req)
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

        public virtual void call(PxpRequest req, Action<Object> asyncRet)
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
                result = method.Invoke(ctx.refSlots[(int)((Object[])req.parameter)[0]], args);
                if (firstInputParamIndex == 0)
                {
                    asyncRet(result);
                }
            }
            catch(Exception ex)
            {
                asyncRet(ex.InnerException);
            }
        }
        public virtual void writeResult(PxpRequest req)
        {
            var ctx = req.context;
            var obj = req.result;
            switch (returnType)
            {
                //primitive type 
                //boolean
                case 1:
                    ctx.writeInt32((Boolean)obj ? 1 : 0);
                    break;
                //byte
                case 2:
                    ctx.writeInt32((Byte)obj);
                    break;
                //short
                case 3:
                    ctx.writeInt32((Int16)obj);
                    break;
                //int
                case 4:
                    ctx.writeInt32((Int32)obj);
                    break;
                //long
                case 5:
                    ctx.writeInt64((Int64)obj);
                    break;
                //float
                case 6:
                    ctx.writeFloat32((Single)obj);
                    break;
                //double
                case 7:
                    ctx.writeFloat64((Double)obj);
                    break;
                //reference type
                case 8:
                case 9:
                    if (req.result is Exception)
                    {
                        ctx.writeInt32(1);
                    }
                    else
                    {
                        ctx.writeInt32(0);
                    }
                    break;
                //processed by callable
                default:
                    throw new NotImplementedException();
            }
        }

        public int csTypeToSwitchId(Type cstype)
        {
            if (cstype == typeof(bool))
            {
                return 1;
            }
            else if (cstype == typeof(byte))
            {
                return 2;
            }
            else if (cstype == typeof(short))
            {
                return 3;
            }
            else if (cstype == typeof(int))
            {
                return 4;
            }
            else if (cstype == typeof(long))
            {
                return 5;
            }
            else if (cstype == typeof(float))
            {
                return 6;
            }
            else if (cstype == typeof(double))
            {
                return 7;
            }
            else if (cstype == typeof(string))
            {
                return 9;
            }
            else
            {
                return 8;
            }

        }

        public Object readNext(ServerContext ctx, int switchId)
        {
            switch (switchId)
            {
                //primitive type 
                //boolean
                case 1:
                    return ctx.readInt32() != 0;
                //byte
                case 2:
                    return (byte)ctx.readInt32();
                //short
                case 3:
                    return (short)ctx.readInt32();
                //int
                case 4:
                    return ctx.readInt32();
                //long
                case 5:
                    return ctx.readInt64();
                //float
                case 6:
                    return ctx.readFloat32();
                //double
                case 7:
                    return ctx.readFloat64();
                //reference type
                case 8:
                    int addr = ctx.readInt32();
                    return ctx.refSlots[addr].get();
                case 9:
                    //string type
                    return ctx.readNextString();
                default:
                    throw new NotImplementedException();
            }
        }


    }
}
