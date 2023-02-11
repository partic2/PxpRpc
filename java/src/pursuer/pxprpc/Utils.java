package pursuer.pxprpc;

import java.io.EOFException;
import java.io.IOException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;

public class Utils {
	// read enough data or EOFException
	public static void readf(ByteChannel in,ByteBuffer b) throws IOException {
		//java9 method signature change, which may cause error when runing on lower java version(usually happen on android).
		Buffer b2=b;
		for(b2.mark();b2.remaining()>0 && in.isOpen();) {
			if(in.read(b)<0) {
				throw new EOFException();
			}
		}
		b2.reset();
	}

	public static int readInt32(ByteChannel in) throws IOException {
		ByteBuffer b = ByteBuffer.allocate(4);
		readf(in,b);
		return b.order(ByteOrder.LITTLE_ENDIAN).getInt();
	}
	public static long readInt64(ByteChannel in) throws IOException {
		ByteBuffer b = ByteBuffer.allocate(8);
		readf(in,b);
		return b.order(ByteOrder.LITTLE_ENDIAN).getLong();
	}
	public static float readFloat32(ByteChannel in) throws IOException {
		ByteBuffer b = ByteBuffer.allocate(4);
		readf(in,b);
		return b.order(ByteOrder.LITTLE_ENDIAN).getFloat();
	}
	public static double readFloat64(ByteChannel in) throws IOException {
		ByteBuffer b = ByteBuffer.allocate(8);
		readf(in,b);
		return b.order(ByteOrder.LITTLE_ENDIAN).getDouble();
	}

	public static void writef(ByteChannel out,ByteBuffer b) throws IOException {
		Buffer b2=b;
		for(b2.mark();
				b2.remaining()>0 && out.isOpen();
				out.write(b)) {}
		b2.reset();
	}
	public static void writeInt32(ByteChannel out,int d) throws IOException {
		ByteBuffer b=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(d);
		((Buffer)b).flip();
		writef(out,b);
	}
	public static void writeInt64(ByteChannel out,long d) throws IOException {
		ByteBuffer b=ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(d);
		((Buffer)b).flip();
		writef(out,b);
	}
	public static void writeFloat32(ByteChannel out,float d) throws IOException {
		ByteBuffer b=ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putFloat(d);
		((Buffer)b).flip();
		writef(out,b);
	}
	public static void writeFloat64(ByteChannel out,double d) throws IOException {
		ByteBuffer b=ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putDouble(d);
		((Buffer)b).flip();
		writef(out,b);
	}

	public static byte[] bytesGet(ByteBuffer bb,int off,int len) {
		Buffer bb2=bb;
		bb2.mark();
		bb2.position(bb2.position()+off);
		byte[] b = new byte[len];
		bb.get(b);
		bb2.reset();
		return b;
	}
	public static void bytesSet(ByteBuffer bb,int off,int len,byte[] b) {
		Buffer bb2=bb;
		bb2.mark();
		bb2.position(bb2.position()+off);
		bb.put(b);
		bb2.reset();
	}
}
