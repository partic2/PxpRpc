package pxprpc.base;

import java.io.Closeable;

public class PxpRef {
	public int index;
	public Object object1;
	public PxpRef nextFree;
	public PxpRef(int index) {
		this.index=index;
	}
	public Object get() {
		return this.object1;
	}
	public void set(Object obj){this.object1 =obj;}
	public void free(){
		if(object1 !=null && object1 instanceof Closeable){
			try{
				((Closeable) object1).close();
			}catch(Exception ignored){}
		}
		this.object1 =null;
	}
}
