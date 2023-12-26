
import asyncio
import asyncio.locks
import traceback
import struct
from .common import NotNone, encodeToBytes,zero32

import typing 
from typing import Optional
from dataclasses import dataclass

from .common import Serializer

@dataclass
class PxpRequest(object):
    context:Optional['ServerContext']=None
    session:int=0
    opcode:int=0
    destAddr:int=0
    srcAddr:int=0
    funcAddr:int=0
    parameter:typing.Any=None
    result:typing.Any=None
    callable:Optional['PxpCallable']=None
    nextPending:Optional['PxpRequest']=None
    inSequence:bool=False
    
class PxpCallable():
        
    async def readParameter(self,req:PxpRequest):
        'abstract'
    
    async def call(self,req:PxpRequest):
        'abstract'
    
    async def writeResult(self,req:PxpRequest):
        'abstract'

import logging

log1=logging.getLogger(__name__)

class ServerContext(object):
    
    def __init__(self):
        self.refSlots:typing.Dict[int,typing.Any]=dict()
        self.writeLock=asyncio.locks.Lock()
        self.funcMap=dict()
        fillFuncMapBuiltIn(self.funcMap)
        self.running=False
        self.pendingRequests:typing.Dict[int,PxpRequest]
        self.sequenceSession:int=0xffffffff
        self.sequenceMaskBitsCnt:int=0


    def backend1(self,r:asyncio.StreamReader,w:asyncio.StreamWriter):
        self.in2:asyncio.StreamReader=r
        self.out2:asyncio.StreamWriter=w

    def queueRequest(self,r:PxpRequest):
        r.opcode=r.session&0xff
        if self.sequenceSession==0xffffffff or (r.session>>(32-self.sequenceMaskBitsCnt)!=self.sequenceSession):
            asyncio.create_task(self.processRequest(r))
            return
        
        r.inSequence=True
        r2=self.pendingRequests.get(r.session>>8,None)
        if r2==None:
            self.pendingRequests[r.session>>8]=r
            asyncio.create_task(self.processRequest(r))
        else:
            while r2.nextPending!=None:
                r2=r2.nextPending
            r2.nextPending=r
        
    def finishRequest(self,r:PxpRequest):
        if r.inSequence:
            if r.nextPending!=None:
                self.pendingRequests[r.session>>8]=r.nextPending
                return r.nextPending
            else:
                del self.pendingRequests[r.session>>8]
                return None
        else:
            return None
        
    async def push(self,req:PxpRequest):
        self.refSlots[req.destAddr]=req.parameter
        self.out2.write(req.session.to_bytes(4,'little'))

    async def pull(self,req:PxpRequest):
        data=self.refSlots.get(req.srcAddr,None)
        self.out2.write(req.session.to_bytes(4,'little'))
        if isinstance(data,(bytes,bytearray)):
            self.out2.write(len(data).to_bytes(4,'little'))
            self.out2.write(data)
        elif isinstance(data,str):
            data=data.encode('utf-8')
            self.out2.write(len(data).to_bytes(4,'little'))
            self.out2.write(data)
        else:
            self.out2.write(b'\xff\xff\xff\xff')

    async def assign(self,req:PxpRequest):
        self.refSlots[req.destAddr]=self.refSlots.get(req.srcAddr,None)
        self.out2.write(req.session.to_bytes(4,'little'))

    async def unlink(self,req:PxpRequest):
        if req.destAddr in self.refSlots:
            del self.refSlots[req.destAddr]
        self.out2.write(req.session.to_bytes(4,'little'))

    async def call(self,req:PxpRequest):
        req.callable=NotNone(req.callable)
        result=await req.callable.call(req)
        req.result=result
        self.refSlots[req.destAddr]=result
        await self.writeLock.acquire()
        try:
            self.out2.write(req.session.to_bytes(4,'little'))
            await req.callable.writeResult(req)
        finally:
            self.writeLock.release()

    async def getFunc(self,req:PxpRequest):
        funcName:str=self.refSlots.get(req.srcAddr,None)
        if type(funcName)==bytes or type(funcName)==bytearray:
            funcName=funcName.decode('utf-8') # type: ignore
        t1=self.funcMap.get(funcName,None)
        if t1==None:
            delimIndex=funcName.rfind('.')
            if delimIndex>=0:
                t2=self.funcMap.get(funcName[0:delimIndex],None)
                subfuncName=funcName[delimIndex+1:]
                if t2!=None and hasattr(t2,subfuncName):
                    t1=getattr(t2,subfuncName)
                    if t1!=None:
                        req.result=req.destAddr
                        self.refSlots[req.destAddr]=PyCallableWrap(t1,True)
        else:
            req.result=req.destAddr
            self.refSlots[req.destAddr]=PyCallableWrap(t1,False)

        if t1==None:
            req.result=0
            
        await self.writeLock.acquire()
        try:
            self.out2.write(req.session.to_bytes(4,'little'))
            self.out2.write(struct.pack('<I',req.result))
        finally:
            self.writeLock.release()

    async def close(self,req:PxpRequest):
        for t2 in self.refSlots:
            del self.refSlots[t2]
        self.out2.close()

    async def getInfo(self,req:PxpRequest):
        self.out2.write(req.session.to_bytes(4,'little'))
        info1=('server name:pxprpc for python3\n'+
                                'version:1.1\n'+
                                'reference slots size:4096\n').encode('utf-8')
        self.out2.write(struct.pack('<i',len(info1)))
        self.out2.write(info1)

    async def sequence(self,r:PxpRequest):
        self.sequenceSession=r.destAddr
        if self.sequenceSession==0xffffffff:
            for r2 in self.pendingRequests.values():
                r2.nextPending=None
            self.pendingRequests.clear()
        else:
            self.sequenceMaskBitsCnt=self.sequenceSession&0xff
            self.sequenceSession=self.sequenceSession>>(32-self.sequenceMaskBitsCnt)
        
        self.out2.write(r.session.to_bytes(4,'little'))

    async def buffer(self,r:PxpRequest):
        # Not implemented
        pass

    async def processRequest(self,req:PxpRequest):
        handler=[None,self.push,self.pull,self.assign,self.unlink,
        self.call,self.getFunc,self.close,self.getInfo,self.sequence,self.buffer]
        r2:Optional[PxpRequest]=req
        while r2!=None:
            await handler[r2.opcode](r2)
            r2=self.finishRequest(r2)
        

    async def serve(self):
        t1:typing.Any
        t2:typing.Any
        self.running=True
        while(self.running):
            req=PxpRequest(self)
            req.session=int.from_bytes(await self.in2.readexactly(4),'little')
            log1.debug('get session:%s',hex(req.session))
            req.opcode=req.session&0xff
            if req.opcode==1:
                # push
                req.destAddr,size=struct.unpack('<II',await self.in2.readexactly(8))
                req.parameter=await self.in2.readexactly(size)
                self.queueRequest(req)
            elif req.opcode==2:
                #pull
                req.srcAddr=struct.unpack('<I',await self.in2.readexactly(4))[0]
                self.queueRequest(req)
            elif req.opcode==3:
                #assign
                req.destAddr,req.srcAddr=struct.unpack('<II',await self.in2.readexactly(8))
                self.refSlots[req.destAddr]=self.refSlots.get(req.srcAddr,None)
                self.queueRequest(req)
            elif req.opcode==4:
                #unlink
                req.destAddr=struct.unpack('<I',await self.in2.readexactly(4))[0]
                self.queueRequest(req)
            elif req.opcode==5:
                #call
                req.context=self
                req.destAddr,req.funcAddr=struct.unpack('<II',await self.in2.readexactly(8))
                t1=self.refSlots.get(req.funcAddr,None)
                req.callable=t1
                await t1.readParameter(req)
                self.queueRequest(req)
            elif req.opcode==6:
                #getFunc
                req.destAddr,req.srcAddr=struct.unpack('<II',await self.in2.readexactly(8))
                self.queueRequest(req)
            elif req.opcode==7:
                #close
                self.queueRequest(req)
            elif req.opcode==8:
                #getInfo
                self.queueRequest(req)
            elif req.opcode==9:
                #sequence
                req.destAddr=int.from_bytes(await self.in2.readexactly(4),'little')
                self.queueRequest(req)
            elif req.opcode==10:
                #buffer
                self.queueRequest(req)
            else :
                pass
            
        
        
import inspect

class decorator:
    @staticmethod
    def signature(sign):
         def fn2(fn):
            fn._pxprpc__PyCallableWrapSignature=sign
            return fn
         return fn2


class PyCallableWrap(PxpCallable):
    
    
    def __init__(self,c:typing.Callable,classMethod:bool):
        self.tParam=''
        self.tResult=''
        self.callable=c
        if hasattr(c,'_pxprpc__PyCallableWrapSignature'):
            self.signature(c._pxprpc__PyCallableWrapSignature)
        else:
            argsInfo:inspect.FullArgSpec=inspect.getfullargspec(c)
            argsig={
                int:'l',float:'d',bool:'c',bytes:'b',str:'s'
            }
            if hasattr(argsInfo,'args'):
                for argName in (argsInfo.args[1:] if classMethod else argsInfo.args):
                    t1=argsInfo.annotations[argName]
                    self.tParam+=argsig.get(t1,'o')
                    
            if 'return' in argsInfo.annotations:
                t1=argsInfo.annotations['return']
                self.tResult+=argsig.get(t1,'o')
            
    def signature(self,sign:str)->'PyCallableWrap':
        self.tParam,self.tResult=sign.split('->')
        return self
    
    async def readParameter(self,req:PxpRequest):
        req.context=NotNone(req.context)
        req.parameter=[]
        len=int.from_bytes(await req.context.in2.readexactly(4),'little')
        buf=await req.context.in2.readexactly(len&0x7fffffff)
        if self.tParam=='b':
            req.parameter.append(buf)
        else:
            ser=Serializer().prepareUnserializing(buf)
            
            for t1 in self.tParam:
                if t1=='i':
                    req.parameter.append(ser.getInt())
                elif t1=='l':
                    req.parameter.append(ser.getLong())
                elif t1=='f':
                    req.parameter.append(ser.getFloat())
                elif t1=='d':
                    req.parameter.append(ser.getDouble())
                elif t1=='c':
                    req.parameter.append(ser.getVarint()!=0)
                elif t1=='b':
                    req.parameter.append(ser.getBytes())
                elif t1=='s':
                    req.parameter.append(ser.getString())
                else:
                    t2=ser.getInt()
                    t3=req.context.refSlots[t2]
                    req.parameter.append(t3)
    
    async def call(self,req:PxpRequest):
        try:
            r=await self.callable(*req.parameter)
            return r
        except Exception as ex2:
            import traceback
            return ex2
    
    async def writeResult(self,req:PxpRequest):
        assert req.context!=None
        if isinstance(req.result,Exception):
            ser=Serializer().prepareSerializing()
            b=ser.putString(str(req.result)).build()
            req.context.out2.write((len(b)|0x80000000).to_bytes(4,'little'))
            req.context.out2.write(b)
        else:
            if self.tResult=='b':
                req.context.out2.write((len(req.result)).to_bytes(4,'little'))
                req.context.out2.write(req.result)
            else:
                ser=Serializer().prepareSerializing()
                result2=[req.result] if len(self.tResult)<=1 else req.result
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
                        ser.putInt(req.destAddr)
                    else:
                        assert False,'Unreachable'
                
                b=ser.build()
                req.context.out2.write((len(b)).to_bytes(4,'little'))
                req.context.out2.write(b)
        



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
