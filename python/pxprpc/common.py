

from typing import *

zero32=bytes.fromhex('00000000')

import struct

class AbstractIo:
    async def receive(self)->bytes:
        raise NotImplemented()
    
    async def send(self,buf:bytes):
        raise NotImplemented()
    
    def close(self):
        raise NotImplemented()
    

def encodeToBytes(obj,addr32:int):
    t1=type(obj)
    if t1==int:
        return obj.to_bytes(8,'little')
    elif t1==float:
        return struct.pack('<d',obj)
    elif t1==bool:
        return bytes([0,0,0,1]) if obj else bytes([0,0,0,0])
    else:
        return addr32.to_bytes(4,'little')
    
T=TypeVar('T')
def NotNone(t:Optional[T])->T:
    return t; #type:ignore

def pytypeToDeclChar(t:type):
    typemap={
                int:'l',float:'d',bool:'c',bytes:'b',str:'s'
            }
    return typemap.get(t,'o')

class Serializer:
    def prepareSerializing(self)->'Serializer':
        self.buf=bytearray()
        self.pos=0
        return self
    
    def prepareUnserializing(self,buf:bytes)->'Serializer':
        self.buf=buf
        self.pos=0
        return self

    def getInt(self)->int:
        val=struct.unpack_from('<i',self.buf,self.pos)[0]
        self.pos+=4
        return val
    
    def getLong(self)->int:
        val=struct.unpack_from('<q',self.buf,self.pos)[0]
        self.pos+=8
        return val

    def getFloat(self)->float:
        val=struct.unpack_from('<f',self.buf,self.pos)[0]
        self.pos+=4
        return val

    def getDouble(self)->float:
        val=struct.unpack_from('<d',self.buf,self.pos)[0]
        self.pos+=8
        return val
    
    def getVarint(self)->int:
        val=self.buf[self.pos]
        self.pos+=1
        if val==255:
            val=struct.unpack_from('<I',self.buf,self.pos)[0]
            self.pos+=4
        return val
        
    def getBytes(self)->bytes:
        len=self.getVarint()
        val=self.buf[self.pos:self.pos+len]
        self.pos+=len
        return val

    def getString(self)->str:
        return self.getBytes().decode('utf-8')


    def putInt(self,val:int)->'Serializer':
        cast(bytearray,self.buf).extend(struct.pack('<i',val))
        return self
    
    def putLong(self,val:int)->'Serializer':
        cast(bytearray,self.buf).extend(struct.pack('<q',val))
        return self

    def putFloat(self,val:float)->'Serializer':
        cast(bytearray,self.buf).extend(struct.pack('<f',val))
        return self

    def putDouble(self,val:float)->'Serializer':
        cast(bytearray,self.buf).extend(struct.pack('<d',val))
        return self

    def putVarint(self,val:int)->'Serializer':
        b2=cast(bytearray,self.buf)
        if val<255:
            b2.extend([val])
        else:
            b2.extend(struct.pack('<BI',255,val))
        return self

    def putBytes(self,val:bytes)->'Serializer':
        b2=cast(bytearray,self.buf)
        self.putVarint(len(val))
        b2.extend(val)
        return self

    def putString(self,s:str)->'Serializer':
        self.putBytes(s.encode('utf-8'))
        return self

    def build(self)->bytearray:
        return cast(bytearray,self.buf)


class TableSerializer:
    FLAG_NO_HEADER_NAME=1
    def __init__(self):
        self.rows:List[List[Any]]=[]
        self.headerType=None
        self.headerName=None

    def setHeader(self,types:Optional[str],names:Optional[List[str]]):
        self.headerName=names
        self.headerType=types
        return self
    
    def getRow(self,index:int):
        return self.rows[index]
    
    def getRowCount(self):
        return len(self.rows)
    
    def addRow(self,row:List[Any]):
        self.rows.append(row)
        return self

    def load(self,buf:bytes):
        ser=Serializer().prepareUnserializing(buf)
        flag=ser.getInt()
        rowCnt=ser.getVarint()
        headerType=ser.getString()
        colCnt=len(headerType)
        if (flag & TableSerializer.FLAG_NO_HEADER_NAME)==0:
            headerName2 = []
            for i1 in range(colCnt):
                headerName2.append(ser.getString())
            self.headerName=headerName2
        
        for i1 in range(rowCnt):
            self.rows.append([None]*colCnt)
        
        for i1 in range(colCnt):
            typ=headerType[i1]
            if typ=='i':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getInt()
            elif typ=='l':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getLong()
            elif typ=='f':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getFloat()
            elif typ=='d':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getDouble()
            elif typ=='b':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getBytes()
            elif typ=='s':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getString()
            elif typ=='c':
                for i2 in range(rowCnt):
                    self.rows[i2][i1]=ser.getVarint()!=0
            else:
                raise IOError('Unknown Type')
        
        return self

    def build(self):
        ser=Serializer().prepareSerializing()
        i1=0
        if self.headerType==None:
            if len(self.rows)>=1:
                self.headerType=''
                for t1 in self.rows[0]:
                    self.headerType+=pytypeToDeclChar(type(t1))
            else:
                self.headerType=''
        colsCnt=len(self.headerType)
        flag=0
        if self.headerName==None:
            flag|=TableSerializer.FLAG_NO_HEADER_NAME
        
        ser.putInt(flag)
        rowCnt=len(self.rows)
        ser.putVarint(rowCnt)
        ser.putString(self.headerType)
        if self.headerName!=None:
            for e in self.headerName:
                ser.putString(e)

        for i1 in range(colsCnt):
            typ=self.headerType[i1]
            if typ=='i':
                for i2 in range(rowCnt):
                    ser.putInt(self.rows[i2][i1])
            elif typ=='l':
                for i2 in range(rowCnt):
                    ser.putLong(self.rows[i2][i1])
            elif typ=='f':
                for i2 in range(rowCnt):
                    ser.putFloat(self.rows[i2][i1])
            elif typ=='d':
                for i2 in range(rowCnt):
                    ser.putDouble(self.rows[i2][i1])
            elif typ=='b':
                for i2 in range(rowCnt):
                    ser.putBytes(self.rows[i2][i1])
            elif typ=='s':
                for i2 in range(rowCnt):
                    ser.putString(self.rows[i2][i1])
            elif typ=='c':
                for i2 in range(rowCnt):
                    ser.putVarint(1 if self.rows[i2][i1] else 0)
            else:
                raise IOError('Unknown Type')

        return ser.build()
