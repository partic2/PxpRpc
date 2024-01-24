
import asyncio
import asyncio.locks
import traceback
import struct
from .common import NotNone

import typing 
from typing import Optional,Any,Callable,List,cast
from dataclasses import dataclass

from .common import Serializer,AbstractIo

@dataclass
class PxpRequest(object):
    context:Optional['ServerContext']=None
    session:int=0
    opcode:int=0
    parameter:bytes=bytes()
    result:bytes=bytes()
    callableIndex:int=-1
    rejected:Optional[Exception]=None
    nextPending:Optional['PxpRequest']=None
    inSequence:bool=False
    
class PxpCallable():
    async def call(self,req:PxpRequest):
        'abstract'

class PxpRef():
    def __init__(self,index:int):
        self.object:Any=None
        self.onFree:Optional[typing.Callable]=None
        self.nextFree:Optional[PxpRef]=None
        self.index=index


import logging

log1=logging.getLogger(__name__)

RefPoolExpandCount=256

class ServerContext(object):

    
    def __init__(self):
        self.refPool:List[PxpRef]=[]
        self.freeRefEntry:Optional[PxpRef]=None
        self.funcMap=dict()
        fillFuncMapBuiltIn(self.funcMap)
        self.running=False
        self.pendingRequests:typing.Dict[int,PxpRequest]
        self.sequenceSession:int=0xffffffff
        self.sequenceMaskBitsCnt:int=0


    def expandRefPools(self):
        start=len(self.refPool);
        end=len(self.refPool)+RefPoolExpandCount;
        for t1 in range(start,end+1):
            self.refPool.append(PxpRef(t1))

        for t1 in range(start,end-1):
            self.refPool[t1].nextFree=self.refPool[t1+1]
        
        self.refPool[end].nextFree=self.freeRefEntry
        self.freeRefEntry=self.refPool[start]

    def allocRef(self):
        if self.freeRefEntry==None:
            self.expandRefPools()
        
        ref2 = NotNone(self.freeRefEntry)
        self.freeRefEntry=ref2.nextFree;
        return ref2

    def freeRef(self,ref2:PxpRef):
        if ref2.onFree!=None:
            ref2.onFree()
            ref2.onFree=None
        ref2.object=None
        ref2.nextFree=self.freeRefEntry
        self.freeRefEntry=ref2

    def getRef(self,index)->PxpRef:
        return self.refPool[index]

    def backend1(self,io1:AbstractIo):
        self.io1=io1

    def queueRequest(self,r:PxpRequest):
        if self.sequenceSession==0xffffffff or (r.session>>(32-self.sequenceMaskBitsCnt)!=self.sequenceSession):
            asyncio.create_task(self.processRequest(r))
            return
        
        r.inSequence=True
        r2=self.pendingRequests.get(r.session,None)
        if r2==None:
            self.pendingRequests[r.session]=r
            asyncio.create_task(self.processRequest(r))
        else:
            while r2.nextPending!=None:
                r2=r2.nextPending
            r2.nextPending=r
        
    def finishRequest(self,r:PxpRequest):
        if r.inSequence:
            if r.nextPending!=None:
                self.pendingRequests[r.session]=r.nextPending
                return r.nextPending
            else:
                del self.pendingRequests[r.session]
                return None
        else:
            return None
        

    async def getFunc(self,req:PxpRequest):
        funcName:str=req.parameter.decode('utf-8')
        t1=self.funcMap.get(funcName,None)
        req.result=(-1).to_bytes(4,'little',signed=True)
        if t1==None:
            delimIndex=funcName.rfind('.')
            if delimIndex>=0:
                t2=self.funcMap.get(funcName[0:delimIndex],None)
                subfuncName=funcName[delimIndex+1:]
                if t2!=None and hasattr(t2,subfuncName):
                    t1=getattr(t2,subfuncName)
                    if t1!=None:
                        ref2=self.allocRef()
                        ref2.object=PyCallableWrap(t1,True)
                        req.result=ref2.index.to_bytes(4,'little')
        else:
            ref2=self.allocRef()
            ref2.object=PyCallableWrap(t1,True)
            req.result=ref2.index.to_bytes(4,'little')

    def close(self):
        self.running=False
        for t2 in self.refPool:
            if t2.onFree!=None:
                t2.onFree()
            t2.object=None
            t2.onFree=None
        self.io1.close()
                

    async def closeHandler(self,req:PxpRequest):
        self.close()

    async def getInfo(self,req:PxpRequest):
        req.result=('server name:pxprpc for python3\n'+
                                'version:2.0\n').encode('utf-8')

    async def sequence(self,req:PxpRequest):
        self.sequenceSession=int.from_bytes(req.parameter,'little')
        if self.sequenceSession==0xffffffff:
            for r2 in self.pendingRequests.values():
                r2.nextPending=None
            self.pendingRequests.clear()
        else:
            self.sequenceMaskBitsCnt=self.sequenceSession&0xff
            self.sequenceSession=self.sequenceSession>>(32-self.sequenceMaskBitsCnt)
        

    async def freeRefHandler(self,req:PxpRequest):
        bytesCnt=len(req.parameter)>>2
        for t1 in range(0,bytesCnt,4):
            self.freeRef(self.getRef(int.from_bytes(req.parameter[t1:t1+4],'little',signed=True)))

    async def processRequest(self,req:PxpRequest):
        handler=[None,self.getFunc,self.freeRefHandler,self.closeHandler,self.getInfo,self.sequence]
        r2:Optional[PxpRequest]=req
        while r2!=None:
            log1.debug('processing:%s,%s',req.session,req.callableIndex)
            if r2.callableIndex>=0:
                await cast(PxpCallable,self.getRef(req.callableIndex).object).call(req)
            else:
                await handler[-r2.callableIndex](r2)
            if req.rejected!=None:
                await self.io1.send((req.session^0x80000000).to_bytes(4,'little')+str(req.rejected).encode('utf-8'))
            else:
                await self.io1.send(req.session.to_bytes(4,'little')+req.result)
            r2=self.finishRequest(r2)
        

    async def serve(self):
        t1:typing.Any
        t2:typing.Any
        self.running=True
        while(self.running):
            req=PxpRequest(self)
            buf=await self.io1.receive()
            req.session,req.callableIndex=struct.unpack('<Ii',buf[:8])
            log1.debug('get session:%s',hex(req.session))
            req.parameter=buf[8:]
            self.queueRequest(req)
            
        
        
import inspect

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
    
    def __init__(self,c:typing.Callable,classMethod:bool):
        self.tParam=''
        self.tResult=''
        self.callable=c
        if hasattr(c,'_pxprpc__PyCallableWrapTypedecl'):
            self.typedecl(c._pxprpc__PyCallableWrapTypedecl)
        else:
            from .common import pytypeToDeclChar
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
            for t1 in self.tParam:
                if t1=='i':
                    para.append(ser.getInt())
                elif t1=='l':
                    para.append(ser.getLong())
                elif t1=='f':
                    para.append(ser.getFloat())
                elif t1=='d':
                    para.append(ser.getDouble())
                elif t1=='c':
                    para.append(ser.getVarint()!=0)
                elif t1=='b':
                    para.append(ser.getBytes())
                elif t1=='s':
                    para.append(ser.getString())
                elif t1=='o':
                    idx=ser.getInt()
                    if idx==-1:
                        para.append(None)
                    else:
                        para.append(req.context.getRef(idx).object)
                else:
                    assert False,'Unreachable'
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
            for t1,t2 in zip(self.tResult,result2):
                if t1=='i':
                    ser.putInt(t2)
                elif t1=='l':
                    ser.putLong(t2)
                elif t1=='f':
                    ser.putFloat(t2)
                elif t1=='d':
                    ser.putDouble(t2)
                elif t1=='b':
                    ser.putBytes(t2)
                elif t1=='c':
                    ser.putVarint(1 if t2 else 0)
                elif t1=='s':
                    ser.putString(t2)
                elif t1=='o':
                    if t2==None:
                        ser.putInt(-1)
                    else:
                        ref2=allocRefFor(req.context,t2)
                        ser.putInt(ref2.index)
                else:
                    assert False,'Unreachable'
            
            b=ser.build()
            return b
        



def fillFuncMapBuiltIn(funcMap:typing.Dict):
    class builtinFuncs:
        async def anyToString(self,obj:object):
            return str(obj)

        async def listLength(self,obj:typing.List):
            return len(obj)
        
        async def listElemAt(self,obj:typing.List,index:int):
            return obj[index]
        
        async def listAdd(self,obj:typing.List,elem:object):
            return obj.append(elem)

        async def listRemove(self,obj:typing.List,index:int)->object:
            return obj.pop(index)

        async def listStringJoin(self,obj:typing.List,sep:str):
            return sep.join(obj)

        async def listBytesJoin(self,obj:typing.List,sep:bytes):
            return sep.join(obj)

    funcMap['builtin']=builtinFuncs()
