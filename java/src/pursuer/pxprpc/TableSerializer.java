package pursuer.pxprpc;

import java.nio.ByteBuffer;
import java.util.ArrayList;

public class TableSerializer {
    public static final int FLAG_NO_HEADER_NAME=1;
    protected String[] headerName=null;
    protected String headerType=null;
    protected ArrayList<Object[]> rows=new ArrayList<Object[]>();
    public TableSerializer setHeader(String types, String[] names){
        headerName=names;
        headerType=types;
        return this;
    }
    public String[] getHeaderName(){
        return headerName;
    }
    public Object[] getRow(int index){
        return this.rows.get(index);
    }
    public int getRowCount() {
        return this.rows.size();
    }
    public TableSerializer addRow(Object[] row){
        this.rows.add(row);return this;
    }
    public TableSerializer load(ByteBuffer buf){
        ser=new Serializer2().prepareUnserializing(buf);
        int flag=ser.getInt();
        int rowCnt=ser.getInt();
        headerType=ser.getString();
        int colCnt=headerType.length();
        if((flag & FLAG_NO_HEADER_NAME)==0){
            ArrayList<String> headerName2 = new ArrayList<String>();
            for(int i1=0;i1<colCnt;i1++){
                headerName2.add(ser.getString());
            }
            headerName=headerName2.toArray(new String[0]);
        }
        for(int i1=0;i1<rowCnt;i1++){
            rows.add(new Object[colCnt]);
        }
        for(int i1=0;i1<colCnt;i1++){
            char type=headerType.charAt(i1);
            switch(type){
                case 'i':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getInt();
                    }
                    break;
                case 'l':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getLong();
                    }
                    break;
                case 'f':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getFloat();
                    }
                    break;
                case 'd':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getDouble();
                    }
                    break;
                case 'b':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getBytes();
                    }
                    break;
                case 's':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getString();
                    }
                    break;
                default:
                    throw new RuntimeException("Unknown Type");
            }
        }
        return this;
    }


    protected Serializer2 ser;
    public ByteBuffer build(){
        ser=new Serializer2().prepareSerializing(64);
        int i1=0;
        int colsCnt=headerType.length();
        int flag=0;
        if(headerName==null){
            flag|=FLAG_NO_HEADER_NAME;
        }
        ser.putInt(flag);
        int rowCnt=rows.size();
        ser.putInt(rowCnt);
        ser.putString(headerType);
        if(headerName!=null){
            for(String e:headerName){
                ser.putString(e);
            }
        }
        for(i1=0;i1<colsCnt;i1++){
            char type=headerType.charAt(i1);
            switch(type){
                case 'i':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putInt((int)rows.get(i2)[i1]);
                    }
                    break;
                case 'l':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putLong((long)rows.get(i2)[i1]);
                    }
                    break;
                case 'f':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putFloat((float)rows.get(i2)[i1]);
                    }
                    break;
                case 'd':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putDouble((double)rows.get(i2)[i1]);
                    }
                    break;
                case 'b':
                    for(int i2=0;i2<rowCnt;i2++){
                        byte[] b=(byte[])rows.get(i2)[i1];
                        ser.putBytes(b,0,b.length);
                    }
                    break;
                case 's':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putString((String)rows.get(i2)[i1]);
                    }
                    break;
                default:
                    throw new RuntimeException("Unknown Type");
            }
        }
        return ser.build();
    }
}
