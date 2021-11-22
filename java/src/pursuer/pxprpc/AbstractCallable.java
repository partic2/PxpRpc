package pursuer.pxprpc;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public abstract class AbstractCallable implements PxpCallable{

	protected ServerContext ctx;
	@Override
	public void context(ServerContext ctx) {
		this.ctx=ctx;
	}
	
	public int readInt32() throws IOException {
		return ctx.readInt32();
	}
	public long readInt64() throws IOException {
		return ctx.readInt64();
	}
	public float readFloat32() throws IOException {
		return ctx.readFloat32();
	}
	public double readFloat64() throws IOException {
		return ctx.readFloat64();
	}
	public byte[] readNextRaw() throws IOException {
		return ctx.readNextRaw();
	}
	public String readNextString() throws IOException {
		return ctx.readNextString();
	}
	
	@Override
	public void writeResult(Object result, int resultAddr) throws IOException {
		if(result==null) {
			ctx.writeInt32(0);
		}else {
			writeNext(javaTypeToSwitchId(result.getClass()), result, resultAddr);
		}
	}

	public int javaTypeToSwitchId(Class<?> jtype) {
		if(jtype.isPrimitive()) {
			String name = jtype.getName();
			if(name.equals("boolean")) {
				return 1;
			}else if(name.equals("byte")) {
				return 2;
			}else if(name.equals("short")) {
				return 3;
			}else if(name.equals("int")) {
				return 4;
			}else if(name.equals("long")) {
				return 5;
			}else if(name.equals("float")) {
				return 6;
			}else if(name.equals("double")) {
				return 7;
			}else{
				return 8;
			}
		}else{
			if(jtype.equals(String.class)) {
				return 9;
			}
			return 8;		
		}
	}
	
	public Object readNext(int switchId) throws IOException {
		switch(switchId) {
		//primitive type 
			//boolean
		case 1:
			return readInt32()!=0;
			//byte
		case 2:
			return (byte)readInt32();
			//short
		case 3:
			return (short)readInt32();
			//int
		case 4:
			return readInt32();
			//long
		case 5:
			return readInt64();
			//float
		case 6:
			return readFloat32();
			//double
		case 7:
			return readFloat64();
		//reference type
		case 8:
			int addr=readInt32();
			return ctx.refSlots.get(addr);
		case 9:
		//string type
			return readNextString();
		default :
			throw new UnsupportedOperationException();
		}
	}

	public void writeNext(int switchId,Object obj,int addrIfRefType) throws IOException {
		switch(switchId) {
		//primitive type 
			//boolean
		case 1:
			ctx.writeInt32((Boolean)obj?1:0);
			break;
			//byte
		case 2:
			ctx.writeInt32((Byte)obj);
			break;
			//short
		case 3:
			ctx.writeInt32((Short)obj);
			break;
			//int
		case 4:
			ctx.writeInt32((Integer)obj);
			break;
			//long
		case 5:
			ctx.writeInt64((Long)obj);
			break;
			//float
		case 6:
			ctx.writeFloat32((Float)obj);
			break;
			//double
		case 7:
			ctx.writeFloat64((Double)obj);
			break;
		//reference type
		case 8:
		case 9:
			ctx.writeInt32(addrIfRefType);
			break;
		default :
			throw new UnsupportedOperationException();
		}
	}
}
