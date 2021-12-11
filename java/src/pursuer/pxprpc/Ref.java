package pursuer.pxprpc;

import java.io.Closeable;
import java.io.IOException;

public class Ref implements Closeable{
	protected int count;
	protected Object content;
	public Ref() {
	}
	public Ref(Object c) {
		this.count=0;
		this.content=c;
	}
	public int addRef() {
		return ++this.count;
	}
	public int release() {
		this.count--;
		if(this.count==0) {
			try {
				this.close();
			} catch (IOException e) {
			}
		}
		return this.count;
	}
	public Object get() {
		return this.content;
	}
	public void close() throws IOException {
		if(content instanceof Closeable) {
			((Closeable) content).close();
		}
	}
}
