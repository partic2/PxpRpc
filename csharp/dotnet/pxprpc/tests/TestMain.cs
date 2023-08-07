using System;
using System.Collections.Generic;
using System.Text;

using pxprpc;
using pxprpc.backend;

namespace pxprpc.tests
{
    class TestFuncMap{
        public string get1234()
        {
            return "1234";
        }
        public void printString(string s)
        {
            Console.WriteLine(s);
        }
        public void wait1Sec(Action<Object> done)
        {
            var tim=new System.Timers.Timer(1000);
            tim.AutoReset = false;
            tim.Elapsed += (object sender, System.Timers.ElapsedEventArgs e) =>
            {
                done(null);
            };
            tim.Start();
        }
        public string raiseError1()
        {
            throw new Exception("dummy io error");
        }


    }
    public class TestMain
    {
        public static void Main(string[] args)
        {
            TcpBackend tcp = new TcpBackend();
            tcp.funcMap["test1"] = new TestFuncMap();
            tcp.bindAddr = "0.0.0.0:2050";
            tcp.listenAndServe();
        }
    }
}
