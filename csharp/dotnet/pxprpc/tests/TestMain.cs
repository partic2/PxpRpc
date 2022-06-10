using System;
using System.Collections.Generic;
using System.Text;

using pxprpc;
using pxprpc.backend;

namespace pxprpc.tests
{
    class TestFuncMap{
        public void printString(string s)
        {
            Console.WriteLine(s);
        }
        public void printStringUnderline(string s)
        {
            Console.WriteLine("__"+s);
        }
    }
    public class TestMain
    {
        public static void Main(string[] args)
        {
            TcpBackend tcp = new TcpBackend();
            tcp.funcMap["test"] = new TestFuncMap();
            tcp.bindAddr = "0.0.0.0:2050";
            tcp.listenAndServe();
        }
    }
}
