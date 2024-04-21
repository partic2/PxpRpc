

from typing import Optional,Any,Callable,List,cast,TypeVar,Dict,Tuple,Set,Union,Awaitable
import logging
log1=logging.getLogger(__name__)
import asyncio
from .base import NotNone,PxpCallable,Serializer,PxpRequest,PxpRef,ServerContext,ClientContext
import struct
import inspect


def pytypeToDeclChar(t):
    typemap={
                int:'l',float:'d',bool:'c',bytes:'b',str:'s'
            }
    return typemap.get(t,'o')

class TableSerializer:
    FLAG_NO_HEADER_NAME=1
    def __init__(self):
        self.rows:List[List[Any]]=[]
        self.headerType=None
        self.headerName=None
        self.ser=None
        self.boundServContext=None
        self.boundClieContext=None

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
    
    def bindContext(self,serv:Optional[ServerContext],clie:Optional['RpcExtendClient1'])->'TableSerializer':
        self.boundServContext=serv
        self.boundClieContext=clie
        return self
    
    def bindSerializer(self,ser:Serializer)->'TableSerializer':
        self.ser=ser
        return self
    
    def getRowsData(self,rowCnt:int)->List[List[Any]]:
        headerType=NotNone(self.headerType)
        colCnt=len(headerType)
        ser=NotNone(self.ser)
        rows=[]
        for i1 in range(rowCnt):
            rows.append([None]*colCnt)
        for i1 in range(colCnt):
            typ=headerType[i1]
            if typ=='i':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getInt()
            elif typ=='l':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getLong()
            elif typ=='f':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getFloat()
            elif typ=='d':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getDouble()
            elif typ=='b':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getBytes()
            elif typ=='s':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getString()
            elif typ=='c':
                for i2 in range(rowCnt):
                    rows[i2][i1]=ser.getVarint()!=0
            elif typ=='o':
                for i2 in range(rowCnt):
                    idx=ser.getInt()
                    if idx==-1:
                        rows[i2][i1]=None
                    else:
                        if self.boundServContext!=None:
                            rows[i2][i1]=self.boundServContext.getRef(idx).object
                        else:
                            rows[i2][i1]=RpcExtendClientObject(NotNone(self.boundClieContext),idx)
            else:
                raise IOError('Unknown Type')
        return rows

    def putRowsData(self,rows:List[List[Any]]):
        headerType=NotNone(self.headerType)
        colsCnt=len(headerType)
        rowCnt=len(rows)
        ser=NotNone(self.ser)
        for i1 in range(colsCnt):
            typ=headerType[i1]
            if typ=='i':
                for i2 in range(rowCnt):
                    ser.putInt(rows[i2][i1])
            elif typ=='l':
                for i2 in range(rowCnt):
                    ser.putLong(rows[i2][i1])
            elif typ=='f':
                for i2 in range(rowCnt):
                    ser.putFloat(rows[i2][i1])
            elif typ=='d':
                for i2 in range(rowCnt):
                    ser.putDouble(rows[i2][i1])
            elif typ=='b':
                for i2 in range(rowCnt):
                    ser.putBytes(rows[i2][i1])
            elif typ=='s':
                for i2 in range(rowCnt):
                    ser.putString(rows[i2][i1])
            elif typ=='c':
                for i2 in range(rowCnt):
                    ser.putVarint(1 if rows[i2][i1] else 0)
            elif typ=='o':
                for i2 in range(rowCnt):
                    t2=rows[i2][i1]
                    if t2==None:
                        ser.putInt(-1)
                    else:
                        if self.boundServContext!=None:
                            ref2=allocRefFor(self.boundServContext,t2)
                            ser.putInt(ref2.index)
                        else:
                            ser.putInt(NotNone(cast(RpcExtendClientObject,t2).value))
            else:
                raise IOError('Unknown Type')

    def load(self,buf:bytes):
        if buf!=None:
            self.bindSerializer(Serializer().prepareUnserializing(buf))
        ser=NotNone(self.ser)
        flag=ser.getVarint()
        rowCnt=ser.getVarint()
        headerType=ser.getString()
        self.headerType=headerType
        colCnt=len(headerType)
        if (flag & TableSerializer.FLAG_NO_HEADER_NAME)==0:
            headerName2 = []
            for i1 in range(colCnt):
                headerName2.append(ser.getString())
            self.headerName=headerName2
        self.rows=self.getRowsData(rowCnt)
        return self

    def build(self):
        if self.ser==None:
            self.ser=Serializer().prepareSerializing()
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
        
        self.ser.putVarint(flag)
        rowCnt=len(self.rows)
        self.ser.putVarint(rowCnt)
        self.ser.putString(self.headerType)
        if self.headerName!=None:
            for e in self.headerName:
                self.ser.putString(e)
        self.putRowsData(self.rows)
        return self.ser.build()
    
    def toMapArray(self)->List[Dict[str,Any]]:
        r=[]
        assert self.headerName!=None
        for t1 in range(self.getRowCount()):
            r0={}
            row=self.getRow(t1)
            for t2,t3 in enumerate(self.headerName):
                r0[t3]=row[t2]
            r.append(r0)
        return r

    def fromMapArray(self,val:List[Dict[str,Any]]):
        if len(val)>=1 and self.headerName==None:
            self.headerName=list(val[0].keys())
        for t1 in val:
            row=[]
            for t2 in NotNone(self.headerName):
                row.append(t1[t2])
            self.addRow(row)
        return self
    
    def toArray(self)->List[Dict[str,Any]]:
        r=[]
        assert self.headerName!=None
        for t1 in range(self.getRowCount()):
            r.append(self.getRow(t1)[0])
        return r

    def fromArray(self,val:List[Dict[str,Any]]):
        if len(val)>=1 and self.headerName==None:
            self.headerName=list(val[0].keys())
        for t1 in val:
            self.addRow([t1])
        return self
    
    


class RpcExtendClientObject():
    def __init__(self,client:'RpcExtendClient1',value:Optional[int]=None):
        self.value=value
        self.client=client

    async def free(self):
        if self.value is not None:
            val=self.value
            self.value=None
            sid=self.client.allocSid()
            try:
                await self.client.conn.freeRef([val],sid)
            finally:
                self.client.freeSid(sid)
            

    async def asCallable(self):
        '''this object will be invalid after this function. use return value instead.'''
        c1=RpcExtendClientCallable(self.client,self.value)
        self.value=None
        return c1
            
    def __del__(self):
        if self.client.conn.running:
            asyncio.create_task(self.free())



class RpcExtendClientCallable(RpcExtendClientObject):
        
        
    def typedecl(self,decl:str):
        ''' function type declaration
format: 'parameters type->return type' 
eg:
a function defined in c:
    bool fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
defined in java:
    boolean fn(int,int,double,Object)
    ...
it's pxprpc typedecl: 
    iido->z

available type typedecl characters:
  i  int(32bit integer)
  l  long(64bit integer)
  f  float(32bit float)
  d  double(64bit float)
  o  object(32bit reference address)
  b  bytes(bytes buffer)

  c  boolean(pxprpc use 1byte(1/0) to store a boolean value)
  s  string(bytes will be decode to string)
        '''
        self.tParam,self.tResult=decl.split('->')
        return self


    async def __call__(self,*args)->Any:
        assert self.value is not None
        if self.tParam=='b':
            packed=args[0]
        else:
            ser=Serializer().prepareSerializing()
            TableSerializer()\
                .bindContext(None,self.client).bindSerializer(ser).setHeader(self.tParam,None)\
                .putRowsData([list(args)])

            packed=ser.build()
        sid=self.client.allocSid()
        try:
            result=await self.client.conn.call(self.value,packed,sid)
            if self.tResult=='b':
                return result
            else:
                ser=Serializer().prepareUnserializing(result)
                results=TableSerializer()\
                    .bindContext(None,self.client).bindSerializer(ser).setHeader(self.tResult,None)\
                    .getRowsData(1)[0]

                if len(self.tResult)==0:
                    return None
                elif len(self.tResult)==1:
                    return results[0]
                else:
                    return results
        finally:
            self.client.freeSid(sid)




class RpcExtendError(Exception):
    pass

class RpcExtendClient1:
    def __init__(self,conn:ClientContext):
        self.conn=conn
        self.__usedSid:Set[int]=set()
        self.__sidStart=1
        self.__sidEnd=0xffff
        self.__nextSid=self.__sidStart
        self.builtIn=None
        self.serverName=None

    async def init(self):
        asyncio.create_task(self.conn.run())
        info=await self.conn.getInfo()
        for t1 in info.split('\n'):
            if ':' in t1:
                key,value=t1.split(':')
                if key=='server name':
                    self.serverName=value

    def allocSid(self)->int:
        reachEnd=False
        while self.__nextSid in self.__usedSid:
            self.__nextSid+=1
            if self.__nextSid>=self.__sidEnd:
                if reachEnd:
                    raise RpcExtendError('No sid available')
                else:
                    reachEnd=True
                    self.__nextSid=self.__sidStart

        t1=self.__nextSid
        self.__nextSid+=1
        if self.__nextSid>=self.__sidEnd:
            self.__nextSid=self.__sidStart
        self.__usedSid.add(t1)
        return t1

    def freeSid(self,index:int):
        try:
            self.__usedSid.remove(index)
        except KeyError:
            pass

    async def getFunc(self,name:str)->Optional[RpcExtendClientCallable]:
        sid=self.allocSid()
        try:
            index=await self.conn.getFunc(name,sid)
            if index==-1:
                return None
            return RpcExtendClientCallable(self,value=index)
        finally:
            self.freeSid(sid)


def allocRefFor(context:ServerContext,obj:Any)->PxpRef:
    ref=context.allocRef()
    ref.object=obj
    if hasattr(obj,'close'):
        def fn():
            obj.close()
        ref.onFree=fn
    return ref

class decorator:
    @staticmethod
    def typedecl(decl):
         def fn2(fn):
            fn._pxprpc__PyCallableWrapTypedecl=decl
            return fn
         return fn2


class PyCallableWrap(PxpCallable):
    
    def __init__(self,c:Callable,classMethod:bool):
        self.tParam=''
        self.tResult=''
        self.callable=c
        if hasattr(c,'_pxprpc__PyCallableWrapTypedecl'):
            self.typedecl(c._pxprpc__PyCallableWrapTypedecl)
        else:
            argsInfo:inspect.FullArgSpec=inspect.getfullargspec(c)
            if hasattr(argsInfo,'args'):
                for argName in (argsInfo.args[1:] if classMethod else argsInfo.args):
                    t1=argsInfo.annotations[argName]
                    self.tParam+=pytypeToDeclChar(t1)
                    
            if 'return' in argsInfo.annotations:
                t1=argsInfo.annotations['return']
                self.tResult+=pytypeToDeclChar(t1)
            
    def typedecl(self,decl:str)->'PyCallableWrap':
        self.tParam,self.tResult=decl.split('->')
        return self
    
    def readParameter(self,req:PxpRequest)->List[Any]:
        assert req.context!=None
        para=[]
        if self.tParam=='b':
            para.append(req.parameter)
        else:
            ser=Serializer().prepareUnserializing(req.parameter)
            para=TableSerializer()\
                .bindContext(req.context,None).bindSerializer(ser).setHeader(self.tParam,None)\
                .getRowsData(1)[0]
        return para
    
    async def call(self,req:PxpRequest):
        para=self.readParameter(req)
        try:
            r=await self.callable(*para)
            req.result=self.writeResult(req,r)
        except Exception as ex:
            req.rejected=ex
    
    def writeResult(self,req:PxpRequest,result:Any)->bytes:
        assert req.context!=None
        if self.tResult=='b':
            return cast(bytes,result)
        else:
            ser=Serializer().prepareSerializing()
            result2=[result] if len(self.tResult)<=1 else result
            TableSerializer()\
                .bindContext(req.context,None).bindSerializer(ser).setHeader(self.tResult,None)\
                .putRowsData([result2])
            b=ser.build()
            return b
        
RegisteredFuncMap:Dict[str,Any]=dict()

def CDefaultExtendFuncMap(name:str)->Optional[PxpCallable]:
    t2=RegisteredFuncMap.get(name,None)
    if t2!=None:
        if isinstance(t2,PxpCallable):
            return t2
        else:
            return PyCallableWrap(t2,False)
    
    delimIndex=name.rfind('.')
    if delimIndex>=0:
        t2=RegisteredFuncMap.get(name[0:delimIndex],None)
        subfuncName=name[delimIndex+1:]
        if t2!=None and hasattr(t2,subfuncName):
            t1=getattr(t2,subfuncName)
            if t1!=None:
                return PyCallableWrap(t1,True)
    return None



class builtinFuncs:
    def __init__(self):
        self.pxprpcGlobals={}
    async def anyToString(self,obj:object):
        return str(obj)

    async def pyExec(self,code:str):
        exec(code,self.pxprpcGlobals,locals())

    async def pyGlobalGet(self,name:str)->Any:
        return self.pxprpcGlobals[name]
    
    async def pyAwait(self,awaitable:Awaitable):
        await awaitable

    async def bufferData(self,buf:Any)->bytes:
        return buf

RegisteredFuncMap['builtin']=builtinFuncs()

from . import base
base.DefaultFuncMap=CDefaultExtendFuncMap