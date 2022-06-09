using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace pxprpc
{
	public class Ref : IDisposable
	{
	protected int count;
	protected Object content;
	public Ref()
	{
	}
	public Ref(Object c)
	{
		this.count = 0;
		this.content = c;
	}
	public int addRef()
	{
		return ++this.count;
	}
	public int release()
	{
		this.count--;
		if (this.count == 0)
		{
			this.Dispose();
		}
		return this.count;
	}
	public Object get()
	{
		return this.content;
	}
	public void Dispose()
	{
		if(content is IDisposable) {
			((IDisposable)content).Dispose();
		}
	}
}
}
