package pxprpc.base;

import java.io.Closeable;

public class PxpRef {
	public int index;
	public Object object1;
	public Closeable onFree;
	public PxpRef nextFree;
	public PxpRef(int index) {
		this.index=index;
	}
	public Object get() {
		return this.object1;
	}
	public void set(Object obj,Closeable onFree){
		this.object1 =obj;
		this.onFree=onFree;
	}
	public void free(){
		if(onFree!=null){
			try{
				onFree.close();
			}catch(Exception ignored){}
			this.onFree=null;
		}
		this.object1 =null;
	}
}
