package pxprpc.extend;

import pxprpc.base.PxpRef;
import pxprpc.base.Serializer2;
import pxprpc.base.ServerContext;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class TableSerializer {
    public static final int FLAG_NO_HEADER_NAME=1;
    public String[] headerName=null;
    public char[] headerType=null;
    public ServerContext boundServContext;
    protected List<Object[]> cols=new ArrayList<Object[]>();
    protected List<Object[]> rows=new ArrayList<Object[]>();
    protected Serializer2 ser;
    public TableSerializer bindSerializer(Serializer2 ser){
        this.ser=ser;
        return this;
    }
    //types: type string, or null to guess
    //names: header name, or null for FLAG_NO_HEADER_NAME packet.
    public TableSerializer setHeader(String types, String[] names){
        if(types!=null){
            return setHeader2(TypeDeclParser.parseDeclText(types),names);
        }else{
            return setHeader2(null,names);
        }
    }
    public TableSerializer setHeader2(char[] types, String[] names){
        this.headerType=types;
        this.headerName=names;
        return this;
    }
    //Optional, for reference (un)serialize only, client are reserved parameter, should be null now.
    public TableSerializer bindContext(ServerContext serv,Object client){
        this.boundServContext =serv;
        return this;
    }
    public Object[] getRow(int index){
        return this.rows.get(index);
    }
    public int getColIndex(String header){
        for(int i=0;i<headerName.length;i++){
            if(headerName[i].equals(header)){
                return i;
            }
        }
        return -1;
    }
    public int getRowCount() {
        return this.rows.size();
    }
    public TableSerializer addRow(Object[] row){
        this.rows.add(row);return this;
    }
    public PxpRef allocRefFor(Object obj){
        PxpRef ref = boundServContext.allocRef();
        if(obj instanceof Closeable){
            ref.set(obj,(Closeable) obj);
        }else{
            ref.set(obj,null);
        }
        return ref;
    }
    public void putRowsData(List<Object[]> rows){
        char[] types = this.headerType;
        int colsCnt = types.length;
        int rowCnt=rows.size();
        for(int i1=0;i1<colsCnt;i1++){
            char type=types[i1];
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
                        ByteBuffer b=(ByteBuffer)rows.get(i2)[i1];
                        ser.putBytes(b);
                    }
                    break;
                case 's':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putString((String)rows.get(i2)[i1]);
                    }
                    break;
                case 'c':
                    for(int i2=0;i2<rowCnt;i2++){
                        ser.putVarint(((boolean)rows.get(i2)[i1])?1:0);
                    }
                    break;
                case 'o':
                    for(int i2=0;i2<rowCnt;i2++){
                        Object obj=rows.get(i2)[i1];
                        if(obj!=null){
                            ser.putInt(allocRefFor(obj).index);
                        }else{
                            ser.putInt(-1);
                        }
                        break;
                    }
                    break;
                default:
                    throw new RuntimeException("Unknown Type");
            }
        }
    }
    public List<Object[]> getRowsData(int rowCnt){
        char[] types = headerType;
        ArrayList<Object[]> rows=new ArrayList<Object[]>();
        int colCnt=types.length;
        for(int i1=0;i1<rowCnt;i1++){
            rows.add(new Object[colCnt]);
        }
        for(int i1=0;i1<colCnt;i1++){
            char type=types[i1];
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
                case 'c':
                    for(int i2=0;i2<rowCnt;i2++){
                        rows.get(i2)[i1]=ser.getVarint()!=0;
                    }
                    break;
                case 'o':
                    for(int i2=0;i2<rowCnt;i2++){
                        int addr=ser.getInt();
                        Object obj=null;
                        if(addr!=-1){
                            obj = boundServContext.getRef(addr).get();
                        }
                        rows.get(i1)[i1]=obj;
                    }
                    break;
                default:
                    throw new RuntimeException("Unknown Type");
            }
        }
        return rows;
    }
    public TableSerializer load(ByteBuffer buf){
        if(buf!=null){
            ser=new Serializer2().prepareUnserializing(buf);
        }
        int flag=ser.getVarint();
        int rowCnt=ser.getVarint();
        headerType= TypeDeclParser.parseDeclText(ser.getString());
        int colCnt=headerType.length;
        if((flag & FLAG_NO_HEADER_NAME)==0){
            ArrayList<String> headerName2 = new ArrayList<String>();
            for(int i1=0;i1<colCnt;i1++){
                headerName2.add(ser.getString());
            }
            headerName=headerName2.toArray(new String[0]);
        }
        this.rows=getRowsData(rowCnt);
        return this;
    }
    public ByteBuffer build(){
        if(ser==null){
            ser=new Serializer2().prepareSerializing(64);
        }
        int i1=0;
        if(headerType==null){
            //guess type
            if(rows.size()>0){
                Object[] firstRow = rows.get(0);
                char[] types=new char[firstRow.length];
                for(int i=0;i<types.length;i++){
                    types[i]= TypeDeclParser.jtypeToValueInfo(firstRow[i].getClass());
                }
                headerType=types;
            }else{
                headerType=new char[0];
            }
        }
        int colsCnt=headerType.length;
        int flag=0;
        if(headerName==null){
            flag|=FLAG_NO_HEADER_NAME;
        }
        ser.putVarint(flag);
        int rowCnt=rows.size();
        ser.putVarint(rowCnt);
        ser.putString(TypeDeclParser.formatDeclText(headerType));
        if(headerName!=null){
            for(String e:headerName){
                ser.putString(e);
            }
        }
        putRowsData(this.rows);
        return ser.build();
    }
    public List<Map<String,Object>> toMapArray(){
        ArrayList<Map<String,Object>> r=new ArrayList<Map<String,Object>>();
        int rowCount=this.getRowCount();
        int colCount=this.headerName.length;
        for(int t1=0;t1<rowCount;t1++){
            Map<String,Object> r0=new HashMap<String,Object>();
            Object[] row=this.getRow(t1);
            for(int t2=0;t2<colCount;t2++){
                r0.put(this.headerName[t2],row[t2]);
            }
            r.add(r0);
        }
        return r;
    }
    public TableSerializer fromMapArray(List<Map<String,Object>> val){
        if(val.size()>0 && this.headerName==null){
            this.headerName=val.get(0).keySet().toArray(new String[0]);
        }
        int rowCount=val.size();
        int colCount=this.headerName.length;
        for(int t1=0;t1<rowCount;t1++){
            Object[] row=new Object[colCount];
            for(int t2=0;t2<colCount;t2++){
                row[t2]=val.get(t1).get(this.headerName[t2]);
            }
            this.addRow(row);
        }
        return this;
    }
    public List<Object> toArray(){
        ArrayList<Object> r=new ArrayList<Object>();
        int rowCount=this.getRowCount();
        for(int t1=0;t1<rowCount;t1++){
            r.add(getRow(t1)[0]);
        }
        return r;
    }
    public TableSerializer fromArray(List<Object> val){
        int rowCount=val.size();
        for(int t1=0;t1<rowCount;t1++){
            this.addRow(new Object[]{val.get(t1)});
        }
        return this;
    }
}
