package pxprpc.extend;

import pxprpc.base.PxpRef;
import pxprpc.base.Serializer2;
import pxprpc.base.ServerContext;

import java.io.Closeable;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class TableSerializer {
    public static final int FLAG_NO_COLUMN_NAME =1;
    public String[] columnsName =null;
    public char[] columnsType =null;
    public ServerContext boundServContext;
    public RpcExtendClient1 boundClieContext;
    protected List<Object[]> cols=new ArrayList<Object[]>();
    protected List<Object[]> rows=new ArrayList<Object[]>();
    protected Serializer2 ser;
    public TableSerializer bindSerializer(Serializer2 ser){
        this.ser=ser;
        return this;
    }
    //types: type string, or null to guess
    //names: columns name, or null for FLAG_NO_COLUMN_NAME packet.
    public TableSerializer setColumnsInfo(String types, String[] names){
        if(types!=null){
            return setColumnsInfo2(TypeDeclParser.parseDeclText(types),names);
        }else{
            return setColumnsInfo2(null,names);
        }
    }
    public TableSerializer setColumnsInfo2(char[] types, String[] names){
        this.columnsType =types;
        this.columnsName =names;
        return this;
    }
    //Optional, for reference (un)serialize only.
    public TableSerializer bindContext(ServerContext serv,RpcExtendClient1 client){
        this.boundServContext=serv;
        this.boundClieContext=client;
        return this;
    }
    public Object[] getRow(int index){
        return this.rows.get(index);
    }
    public int getColIndex(String name){
        for(int i = 0; i< columnsName.length; i++){
            if(columnsName[i].equals(name)){
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
        char[] types = this.columnsType;
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
                    if(this.boundServContext!=null){
                        for(int i2=0;i2<rowCnt;i2++){
                            Object obj=rows.get(i2)[i1];
                            if(obj!=null){
                                ser.putInt(allocRefFor(obj).index);
                            }else{
                                ser.putInt(-1);
                            }
                            break;
                        }
                    }else{
                        for(int i2=0;i2<rowCnt;i2++){
                            Object obj=rows.get(i2)[i1];
                            if(obj!=null && obj instanceof RpcExtendClientObject){
                                ser.putInt(((RpcExtendClientObject) obj).value);
                            }else{
                                ser.putInt(-1);
                            }
                            break;
                        }
                    }
                    break;
                default:
                    throw new RuntimeException("Unknown Type");
            }
        }
    }
    public List<Object[]> getRowsData(int rowCnt){
        char[] types = columnsType;
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
                            if(this.boundServContext!=null) {
                                obj = boundServContext.getRef(addr).get();
                            }else{
                                obj=new RpcExtendClientObject(this.boundClieContext,addr);
                            }
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
        columnsType = TypeDeclParser.parseDeclText(ser.getString());
        int colCnt= columnsType.length;
        if((flag & FLAG_NO_COLUMN_NAME)==0){
            ArrayList<String> colName2 = new ArrayList<String>();
            for(int i1=0;i1<colCnt;i1++){
                colName2.add(ser.getString());
            }
            columnsName =colName2.toArray(new String[0]);
        }
        this.rows=getRowsData(rowCnt);
        return this;
    }
    public ByteBuffer build(){
        if(ser==null){
            ser=new Serializer2().prepareSerializing(64);
        }
        int i1=0;
        if(columnsType ==null){
            //guess type
            if(rows.size()>0){
                Object[] firstRow = rows.get(0);
                char[] types=new char[firstRow.length];
                for(int i=0;i<types.length;i++){
                    types[i]= TypeDeclParser.jtypeToValueInfo(firstRow[i].getClass());
                }
                columnsType =types;
            }else{
                columnsType =new char[0];
            }
        }
        int colsCnt= columnsType.length;
        int flag=0;
        if(columnsName ==null){
            flag|= FLAG_NO_COLUMN_NAME;
        }
        ser.putVarint(flag);
        int rowCnt=rows.size();
        ser.putVarint(rowCnt);
        ser.putString(TypeDeclParser.formatDeclText(columnsType));
        if(columnsName !=null){
            for(String e: columnsName){
                ser.putString(e);
            }
        }
        putRowsData(this.rows);
        return ser.build();
    }
    public List<Map<String,Object>> toMapArray(){
        ArrayList<Map<String,Object>> r=new ArrayList<Map<String,Object>>();
        int rowCount=this.getRowCount();
        int colCount=this.columnsName.length;
        for(int t1=0;t1<rowCount;t1++){
            Map<String,Object> r0=new HashMap<String,Object>();
            Object[] row=this.getRow(t1);
            for(int t2=0;t2<colCount;t2++){
                r0.put(this.columnsName[t2],row[t2]);
            }
            r.add(r0);
        }
        return r;
    }
    public TableSerializer fromMapArray(List<Map<String,Object>> val){
        if(val.size()>0 && this.columnsName ==null){
            this.columnsName =val.get(0).keySet().toArray(new String[0]);
        }
        int rowCount=val.size();
        int colCount=this.columnsName.length;
        for(int t1=0;t1<rowCount;t1++){
            Object[] row=new Object[colCount];
            for(int t2=0;t2<colCount;t2++){
                row[t2]=val.get(t1).get(this.columnsName[t2]);
            }
            this.addRow(row);
        }
        return this;
    }
    public <T> List<T> toTypedObjectArray(Class<T> cls){
        try {
            //nested object is not support yet.
            Field[] fields=cls.getFields();
            int[] colsIndex=new int[fields.length];
            for(int i=0;i<fields.length;i++){
                colsIndex[i]=this.getColIndex(fields[i].getName());
            }
            ArrayList<T> ret=new ArrayList<T>();
            int rowCount=this.getRowCount();
            for(int i=0;i<rowCount;i++){
                    T t1=cls.newInstance();
                    Object[] row=this.getRow(i);
                    ret.add(t1);
                    for(int i2=0;i2<fields.length;i2++){
                        if(colsIndex[i2]>=0) {
                            fields[i2].set(t1,row[i2]);
                        }
                    }
            }
            return ret;
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
    }
    public TableSerializer fromTypedObjectArray(List<?> objs){
        try {
            Class<?> cls=Object.class;
            if(objs.size()>0){
                cls=objs.get(0).getClass();
            }
            Field[] fields=cls.getFields();
            StringBuilder colTypes=new StringBuilder();
            ArrayList<String> colNames=new ArrayList<String>();
            ArrayList<Field> validField=new ArrayList<Field>();
            for(int i=0;i<fields.length;i++){
                Class<?> typ = fields[i].getType();
                char typCh=TypeDeclParser.jtypeToValueInfo(typ);
                if(typCh=='o'){
                    //nested object is not support yet.
                    continue;
                }
                colTypes.append(typCh);
                colNames.add(fields[i].getName());
                validField.add(fields[i]);
            }
            this.setColumnsInfo(colTypes.toString(),colNames.toArray(new String[0]));
            for(Object e:objs){
                Object[] row = new Object[validField.size()];
                for(int i=0;i<row.length;i++){
                        row[i]=validField.get(i).get(e);
                }
                this.addRow(row);
            }
        } catch (IllegalAccessException ex) {
            throw new RuntimeException(ex);
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
