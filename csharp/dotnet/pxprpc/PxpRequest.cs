using System;
using System.Collections.Generic;
using System.Text;

namespace pxprpc
{
	public class PxpRequest
	{
		public ServerContext context;
		public byte[] session;
		public int opcode;
		public int destAddr;
		public int srcAddr;
		public Object parameter;
		public Object result;
		public PxpCallable callable;
		public bool pending = false;
	}
}
