package pursuer.pxprpc;

import java.io.IOException;

public abstract class AbstractCallable implements PxpCallable{

	
	@Override
	public void writeResult(PxpRequest req) throws IOException{
		ServerContext ctx = req.context;
		if(req.result==null) {
			ctx.writeInt32(0);
		}else {
			writeNext(req.context,javaTypeToSwitchId(req.result.getClass()), req.result, req.destAddr);
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
	
	public Object readNext(ServerContext ctx,int switchId) throws IOException {
		switch(switchId) {
		//primitive type 
			//boolean
		case 1:
			return ctx.readInt32()!=0;
			//byte
		case 2:
			return (byte)ctx.readInt32();
			//short
		case 3:
			return (short)ctx.readInt32();
			//int
		case 4:
			return ctx.readInt32();
			//long
		case 5:
			return ctx.readInt64();
			//float
		case 6:
			return ctx.readFloat32();
			//double
		case 7:
			return ctx.readFloat64();
		//reference type
		case 8:
			int addr=ctx.readInt32();
			return ctx.refSlots[addr].get();
		case 9:
		//string type
			return ctx.readNextString();
		default :
			throw new UnsupportedOperationException();
		}
	}

	public void writeNext(ServerContext ctx,int switchId,Object obj,int addrIfRefType) throws IOException {
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
