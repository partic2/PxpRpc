package pursuer.pxprpc;

import java.io.IOException;

public abstract class AbstractCallable implements PxpCallable{

	
	@Override
	public void writeResult(PxpRequest req) throws IOException{
		ServerContext ctx = req.context;
		if(Exception.class.isInstance(req.result)) {
			ctx.writeInt32(1);
		}else {
			ctx.writeInt32(0);
		}
	}

	public int javaTypeToSwitchId(Class<?> jtype) {
		if(jtype.isPrimitive()) {
			String name = jtype.getName();
			if(name.equals("boolean")) {
				return 1;
			}else if(name.equals("int")) {
				return 2;
			}else if(name.equals("long")) {
				return 3;
			}else if(name.equals("float")) {
				return 4;
			}else if(name.equals("double")) {
				return 5;
			}else{
				return 6;
			}
		}else{
			if(jtype.equals(byte[].class)) {
				return 7;
			}else if(jtype.equals(String.class)) {
				return 8;
			}
			return 6;		
		}
	}
	
	public Object readNext(ServerContext ctx,int switchId) throws IOException {
		switch(switchId) {
		//primitive type 
		case 1: //boolean
			return ctx.readInt32()!=0;
		case 2: //int
			return ctx.readInt32();
		case 3: //long
			return ctx.readInt64();
		case 4: //float
			return ctx.readFloat32();
		case 5: //double
			return ctx.readFloat64();
		//reference type
		case 6:
			int addr=ctx.readInt32();
			return ctx.refSlots[addr].get();
		case 7:
			//byte[]
			return ctx.readNextBytes();
		case 8:
			//String
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
			//int
		case 2:
			ctx.writeInt32((Integer)obj);
			break;
			//long
		case 3:
			ctx.writeInt64((Long)obj);
			break;
			//float
		case 4:
			ctx.writeFloat32((Float)obj);
			break;
			//double
		case 5:
			ctx.writeFloat64((Double)obj);
			break;
		//reference type
		case 6:
		case 7:
		case 8:
			//processed by callable
		default :
			throw new UnsupportedOperationException();
		}
	}
}
