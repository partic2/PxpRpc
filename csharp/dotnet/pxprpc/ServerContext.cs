using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace pxprpc
{
    public class ServerContext
    {
        public Stream stream;
        public static int DefaultRefSlotsCap = 256;
        public PxpObject[] refSlots = new PxpObject[256];
        public Dictionary<String, Object> funcMap = new Dictionary<String, Object>();
        protected BuiltInFuncList builtIn;
        public void init(Stream stream)
        {
            this.stream = stream;
            builtIn = new BuiltInFuncList();
            funcMap["builtin"] = builtIn;
        }
        public bool running = true;

        protected void putRefSlots(int addr, PxpObject r)
        {
            if (refSlots[addr] != null)
                refSlots[addr].release();
            if (r != null)
                r.addRef();
            refSlots[addr] = r;
        }
        private Mutex priv__writeLock = new Mutex();
        public Mutex writeLock()
        {
            return priv__writeLock;
        }
        public void push(PxpRequest r)
        {
            putRefSlots(r.destAddr, new PxpObject(r.parameter));
            writeLock().WaitOne();
            this.stream.Write(r.session);
            writeLock().ReleaseMutex();
            this.stream.Flush();
        }
        public void pull(PxpRequest r)
        {
            Object o = null;
            if (refSlots[r.srcAddr] != null)
            {
                o = refSlots[r.srcAddr].get();
            }
            writeLock().WaitOne();
            this.stream.Write(r.session);
            if (o is byte[])
            {
                byte[] b = (byte[])o;
                writeInt32(b.Length);
                stream.Write(b);
            }
            else if (o is String)
            {
                byte[] b = Encoding.UTF8.GetBytes((String)o);
                writeInt32(b.Length);
                stream.Write(b);
            }
            else
            {
                writeInt32(-1);
            }
            writeLock().ReleaseMutex();
            stream.Flush();
        }
        public void assign(PxpRequest r)
        {
            putRefSlots(r.destAddr, this.refSlots[r.srcAddr]);
            writeLock().WaitOne();
            this.stream.Write(r.session);
            writeLock().ReleaseMutex();
            this.stream.Flush();
        }
        public void unlink(PxpRequest r)
        {
            putRefSlots(r.destAddr, null);
            writeLock().WaitOne();
            this.stream.Write(r.session);
            writeLock().ReleaseMutex();
            this.stream.Flush();
        }
        public void call(PxpRequest r)
        {
            r.pending = true;
            r.callable.call(r, (Object result) =>
         {

             r.result = result;
             putRefSlots(r.destAddr, new PxpObject(result));
             r.pending = false;
             writeLock().WaitOne();
             stream.Write(r.session);
             r.callable.writeResult(r);
             writeLock().ReleaseMutex();
             stream.Flush();

         });
        }


        public void getInfo(PxpRequest r)
        {
            writeLock().WaitOne();
            this.stream.Write(r.session);
            byte[]
            b = Encoding.UTF8.GetBytes(
            "server name:pxprpc for c#\n" +
            "version:1.0\n" +
            "reference slots capacity:" + this.refSlots.Length + "\n"
            );
            writeInt32(b.Length);
            stream.Write(b);
            writeLock().ReleaseMutex();
            stream.Flush();
        }

        public void serve()
        {
            this.running = true;
            while (running)
            {
                PxpRequest r = new PxpRequest();
                r.context = this;
                byte[] session = new byte[4];
                Utils.readf(stream, session);
                r.session = session;
                r.opcode = session[0];
                switch (r.opcode)
                {
                    case 1:
                        r.destAddr = readInt32();
                        int len = readInt32();
                        byte[] buf = new byte[len];
                        Utils.readf(stream, buf);
                        r.parameter = buf;
                        push(r);
                        break;
                    case 2:
                        r.srcAddr = readInt32();
                        pull(r);
                        break;
                    case 3:
                        r.destAddr = readInt32();
                        r.srcAddr = readInt32();
                        assign(r);
                        break;
                    case 4:
                        r.destAddr = readInt32();
                        unlink(r);
                        break;
                    case 5:
                        r.destAddr = readInt32();
                        r.srcAddr = readInt32();
                        r.callable = (PxpCallable)refSlots[r.srcAddr].get();
                        r.callable.readParameter(r);
                        call(r);
                        break;
                    case 6:
                        r.destAddr = readInt32();
                        r.srcAddr = readInt32();
                        getFunc(r);
                        break;
                    case 7:
                        Dispose();
                        running = false;
                        break;
                    case 8:
                        getInfo(r);
                        break;
                }
            }
            running = false;
        }
        public int readInt32()
        {
            return Utils.readInt32(stream);
        }

        public long readInt64()
        {
            return Utils.readInt64(stream);
        }
        public float readFloat32()
        {
            return Utils.readFloat32(stream);
        }
        public double readFloat64()
        {
            return Utils.readFloat64(stream);
        }
        public byte[] readNextRaw()
        {
            int addr = readInt32();
            return (byte[])refSlots[addr].get();
        }
        public void getFunc(PxpRequest r)
        {
            String name = getStringAt(r.srcAddr);
            int namespaceDelim = name.LastIndexOf(".");
            String ns = name.Substring(0, namespaceDelim);
            String func = name.Substring(namespaceDelim + 1);
            Object obj = funcMap[ns];
            PxpCallable found = null;
            if (obj != null)
            {
                found = builtIn.getBoundMethod(obj, func);
            }
            writeLock().WaitOne();
            if (found == null)
            {

                this.stream.Write(r.session);

                writeInt32(0);
            }
            else
            {
                putRefSlots(r.destAddr, new PxpObject(found));

                this.stream.Write(r.session);

                writeInt32(r.destAddr);

            }
            writeLock().ReleaseMutex();
            this.stream.Flush();
        }

        public String readNextString()
        {
            int addr = readInt32();
            return getStringAt(addr);
        }

        public String getStringAt(int addr)
        {
            if (refSlots[addr] == null)
            {
                return "";
            }
            Object o = refSlots[addr].get();
            if (o is byte[])
            {
                return Encoding.UTF8.GetString((byte[])o);
            }
            else
            {
                return o.ToString();
            }
        }

        public void writeInt32(int d)
        {
            Utils.writeInt32(stream, d);
        }
        public void writeInt64(long d)
        {
            Utils.writeInt64(stream, d);
        }
        public void writeFloat32(float d)
        {
            Utils.writeFloat32(stream, d);
        }
        public void writeFloat64(double d)
        {
            Utils.writeFloat64(stream, d);
        }


        public void Dispose()
        {
            running = false;
            stream.Close();
            stream.Dispose();
            for (int i1 = 0; i1 < refSlots.Length; i1++)
            {
                if (refSlots[i1] != null)
                {
                    try
                    {
                        refSlots[i1].release();
                    }
                    catch (Exception e) { }
                }
            }
        }
    }
}
