package pursuer.pxprpc;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Utils {
	public static int readInt32(InputStream in) throws IOException {
		byte[] b=new byte[4];
		in.read(b);
		return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).getInt();
	}
	public static long readInt64(InputStream in) throws IOException {
		byte[] b=new byte[8];
		in.read(b);
		return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).getLong();
	}
	public static float readFloat32(InputStream in) throws IOException {
		byte[] b=new byte[4];
		in.read(b);
		return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).getFloat();
	}
	public static double readFloat64(InputStream in) throws IOException {
		byte[] b=new byte[8];
		in.read(b);
		return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).getDouble();
	}

	
	public static void writeInt32(OutputStream out,int d) throws IOException {
		byte[] b=new byte[4];
		ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).putInt(d);
		out.write(b);
	}
	public static void writeInt64(OutputStream out,long d) throws IOException {
		byte[] b=new byte[8];
		ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).putLong(d);
		out.write(b);
	}
	public static void writeFloat32(OutputStream out,float d) throws IOException {
		byte[] b=new byte[4];
		ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).putFloat(d);
		out.write(b);
	}
	public static void writeFloat64(OutputStream out,double d) throws IOException {
		byte[] b=new byte[8];
		ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).putDouble(d);
		out.write(b);
	}
}
