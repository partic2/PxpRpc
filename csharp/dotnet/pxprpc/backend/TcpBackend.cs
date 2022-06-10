using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace pxprpc.backend
{
    public class TcpWorkThread
    {

        public TcpClient s;
        public ServerContext sc;
        public TcpBackend attached;
        public TcpWorkThread(TcpClient s, ServerContext sc)
        {
            this.s = s;
            this.sc = sc;
        }
        public void run()
        {
            try
            {
                sc.init(s.GetStream());
                sc.serve();
            }
            catch (Exception e)
            {
                //catch all exception to avoid breaking Application unexpectedly.
                if (!(e is IOException))
                {
                    Console.Error.Write(e.Message);
                    Console.Error.Write(e.StackTrace);
                }
            }
            finally
            {
                sc.Dispose();
                if (attached != null)
                {
                    attached.runningContext.Remove(this);
                }
            }

        }
    }
    public class TcpBackend : IDisposable
    {
        public TcpBackend()
        {
        }
        public String bindAddr;
        public Dictionary<String, Object> funcMap = new Dictionary<String, Object>();
        protected TcpListener ss;
        public HashSet<TcpWorkThread> runningContext = new HashSet<TcpWorkThread>();

        protected void execute(Action r)
        {
            Thread t = new Thread(() => r());
            t.IsBackground = true;
            t.Start();
        }
        protected bool running = false;
        public void listenAndServe()
        {
            ss = new TcpListener(IPEndPoint.Parse(bindAddr));
            ss.Start();
            this.running = true;
            while (running)
            {
                var soc = ss.AcceptTcpClient();
                ServerContext sc = new ServerContext();
                sc.funcMap = this.funcMap;
                TcpWorkThread wt = new TcpWorkThread(soc, sc);
                wt.attached = this;
                runningContext.Add(wt);
                execute(wt.run);

            }
        }
        public void Dispose()
        {
            //Copy runningContext to avoid modifing to iterating
            this.running = false;
            List<TcpWorkThread> snapshot = new List<TcpWorkThread>();
            snapshot.AddRange(runningContext);
            foreach (TcpWorkThread wt in snapshot)
            {
                if (wt.sc.running)
                {
                    wt.sc.Dispose();
                }
            }
            ss.Stop();
        }
    }
}
