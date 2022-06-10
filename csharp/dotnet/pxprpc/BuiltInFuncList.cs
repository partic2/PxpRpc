using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;

namespace pxprpc
{
    public class BuiltInFuncList
    {
        public String asString(byte[] utf8)
        {
            return Encoding.UTF8.GetString(utf8);
        }
        public String anyToString(Object obj)
        {
            return obj.ToString();
        }
        public AbstractCallable getMethod(Object obj, String methodName)
        {
            try
            {
                MethodInfo found = null;
                foreach (MethodInfo method in obj.GetType().GetMethods())
                {
                    if (method.Name == methodName)
                    {
                        found = method;
                        break;
                    }
                }
                return new MethodCallable(found);
            }
            catch (Exception e)
            {
                return null;
            }

        }
        public AbstractCallable getBoundMethod(Object obj, String methodName)
        {
            try
            {
                MethodInfo found = null;
                foreach (MethodInfo method in obj.GetType().GetMethods())
                {
                    if (method.Name == methodName)
                    {
                        found = method;
                        break;
                    }
                }
                return new BoundMethodCallable(found, obj);
            }
            catch (Exception e)
            {
                return null;
            }

        }
        public String checkException(Object obj)
        {
            if (obj != null && obj is Exception)
            {
                return obj.ToString();
            }
            else
            {
                return "";
            }
        }
        public Type findClass(String name)
        {
            try
            {
                return Type.GetType(name);
            }
            catch (Exception e)
            {
                return null;
            }
        }
        public Object newObject(Type cls)
        {
            return cls.GetConstructor(new Type[0]).Invoke(new object[0]);

        }

        public int listLength(List<Object> array)
        {
            return array.Count;
        }
        public Object listElemAt(List<Object> array, int index)
        {
            return array[index];
        }
        public void listAdd(List<Object> array, Object obj)
        {
            array.Add(obj);
        }
        public void listRemove(List<Object> array, int index)
        {
            array.Remove(index);
        }
        public byte[] listBytesJoin(List<byte[]> array, byte[] sep)
        {
            int size = array.Count;
            if (size == 0)
            {
                return new byte[0];
            }
            var bais = new MemoryStream();
            bais.Write(array[0]);
            for (int i1 = 1; i1 < size; i1++)
            {
                bais.Write(sep);
                bais.Write(array[i1]);
            }
            return bais.ToArray();
        }
        public String listStringJoin(List<String> array, String sep)
        {
            int size = array.Count;
            if (size == 0)
            {
                return "";
            }
            StringBuilder sb = new StringBuilder();
            sb.Append(array[0]);
            for (int i1 = 1; i1 < size; i1++)
            {
                sb.Append(sep);
                sb.Append(array[i1]);
            }
            return sb.ToString();
        }

    }
}
