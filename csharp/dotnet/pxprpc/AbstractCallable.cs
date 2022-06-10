using System;
using System.Collections.Generic;
using System.Text;

namespace pxprpc
{
    public abstract class AbstractCallable : PxpCallable
    {
        public void writeResult(PxpRequest req)
        {
            ServerContext ctx = req.context;
            if (req.result is Exception)
            {
                ctx.writeInt32(1);
            }
            else
            {
                ctx.writeInt32(0);
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

        public void writeNext(ServerContext ctx, int switchId, Object obj, int addrIfRefType)
        {
            switch (switchId)
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
                //processed by callable
                default:
                    throw new NotImplementedException();
            }
        }

        public abstract void readParameter(PxpRequest req);
        public abstract void call(PxpRequest req, Action<object> asyncRet);
    }
}
