using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace pxprpc
{
    class Utils
    {
        // read enough data or EOFException
        public static void readf(Stream s, byte[] b)
        {
            for (int st = 0; st < b.Length;)
            {
                int r = s.Read(b, st, b.Length - st);
                if (r <= 0)
                {
                    throw new EndOfStreamException();
                }
                st += r;
            }
        }

        public static int readInt32(Stream s)
        {
            byte[] b = new byte[4];
            readf(s, b);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            return BitConverter.ToInt32(b, 0);
        }
        public static long readInt64(Stream s)
        {
            byte[] b = new byte[8];
            readf(s, b);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            return BitConverter.ToInt64(b, 0);
        }
        public static float readFloat32(Stream s)
        {
            byte[]
        b = new byte[4];
            readf(s, b);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            return BitConverter.ToSingle(b);
        }
        public static double readFloat64(Stream s)
        {
            byte[]
        b = new byte[8];
            readf(s, b);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            return BitConverter.ToDouble(b);
        }


        public static void writeInt32(Stream s, int d)
        {
            byte[] b = BitConverter.GetBytes(d);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            s.Write(b, 0, b.Length);
        }
        public static void writeInt64(Stream s, long d)
        {
            byte[] b = BitConverter.GetBytes(d);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            s.Write(b, 0, b.Length);
        }
        public static void writeFloat32(Stream s, float d)
        {
            byte[] b = BitConverter.GetBytes(d);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            s.Write(b, 0, b.Length);
        }
        public static void writeFloat64(Stream s, double d)
        {
            byte[] b = BitConverter.GetBytes(d);
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(b);
            }
            s.Write(b, 0, b.Length);
        }

    }
}
