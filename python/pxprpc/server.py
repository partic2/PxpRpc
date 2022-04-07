
import asyncio
import asyncio.locks
import traceback
import typing
import struct
from .common import encodeToBytes,zero32

from dataclasses import dataclass

@dataclass
class PxpRequest(object):
    context:typing.Union['ServerContext',None]=None
    session:typing.Union[bytes,None]=None
    opcode:int=0
    destAddr:int=0
    srcAddr:int=0
    funcAddr:int=0
    parameter:typing.Any=None
    result:typing.Any=None
    callable:typing.Union['PxpCallable',None]=None
    
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


    def backend1(self,r:asyncio.StreamReader,w:asyncio.StreamWriter):
        self.in2=r
        self.out2=w
        
    async def handle(self):
        self.running=True
        while(self.running):
            session=await self.in2.readexactly(4)
            log1.debug('get session:%s',session)
            if session[0]==1:
                # push
                req=PxpRequest()
                req.destAddr,size=struct.unpack('<II',await self.in2.read(8))
                req.parameter=await self.in2.read(size)
                self.refSlots[req.destAddr]=req.parameter
                log1.debug('server get request:%s',req)
                await self.writeLock.acquire()
                try:
                    self.out2.write(session)
                finally:
                    self.writeLock.release()
            elif session[0]==2:
                #pull
                req=PxpRequest()
                req.srcAddr=struct.unpack('<I',await self.in2.read(4))[0]
                data=self.refSlots.get(req.srcAddr,None)
                log1.debug('server get request:%s',req)
                await self.writeLock.acquire()
                try:
                    self.out2.write(session)
                    if isinstance(data,(bytes,bytearray)):
                        self.out2.write(struct.pack('<I',len(data)))
                        self.out2.write(data)
                    elif isinstance(data,str):
                        data=data.encode('utf-8')
                        self.out2.write(struct.pack('<I',len(data)))
                        self.out2.write(data)
                    else:
                        self.out2.write(bytes([255,255,255,255]))
                finally:
                    self.writeLock.release()
            elif session[0]==3:
                #assign
                req=PxpRequest()
                req.destAddr,req.srcAddr=struct.unpack('<II',await self.in2.read(8))
                self.refSlots[req.destAddr]=self.refSlots.get(req.srcAddr,None)
                log1.debug('server get request:%s',req)
                await self.writeLock.acquire()
                try:
                    self.out2.write(session)
                finally:
                    self.writeLock.release()
            elif session[0]==4:
                #unlink
                req=PxpRequest()
                req.destAddr=struct.unpack('<I',await self.in2.read(4))[0]
                del self.refSlots[req.destAddr]
                log1.debug('server get request:%s',req)
                await self.writeLock.acquire()
                try:
                    self.out2.write(session)
                finally:
                    self.writeLock.release()
            elif session[0]==5:
                #call
                try:
                    req=PxpRequest()
                    req.context=self
                    req.session=session
                    req.destAddr,req.funcAddr=struct.unpack('<II',await self.in2.read(8))
                    t1:PxpCallable=self.refSlots.get(req.funcAddr,None)
                    req.callable=t1
                    await t1.readParameter(req)
                    log1.debug('server get request:%s',req)
                    asyncio.create_task(self.__callRoutine(req))
                except Exception as ex:
                    traceback.print_exc()
                    raise ex
            elif session[0]==6:
                #getFunc
                req=PxpRequest()
                req.session=session
                req.destAddr,req.srcAddr=struct.unpack('<II',await self.in2.read(8))
                funcName=self.refSlots.get(req.srcAddr,None)
                if type(funcName)==bytes or type(funcName)==bytearray:
                    funcName=funcName.decode('utf-8')
                t1=self.funcMap.get(funcName,None)
                if t1==None:
                    req.result=0
                else:
                    req.result=req.destAddr
                    self.refSlots[req.destAddr]=PyCallableWrap(t1)
                log1.debug('server get request:%s',req)
                await self.writeLock.acquire()
                try:
                    self.out2.write(session)
                    self.out2.write(struct.pack('<I',req.result))
                finally:
                    self.writeLock.release()
            elif session[0]==7:
                for t1,t2 in self.refSlots:
                    del self.refSlots[t1]
                self.out2.close()
            elif session[0]==8:
                #getInfo
                log1.debug('server get request:getInfo')
                await self.writeLock.acquire()
                try:
                    self.out2.write(session)
                    info1=('server name:pxprpc for python3\n'+
                                          'version:1.0\n'+
                                          'reference slots size:256\n').encode('utf-8')+bytes([0])
                    self.out2.write(struct.pack('<i',len(info1)))
                    self.out2.write(info1)
                finally:
                    self.writeLock.release()
            else :
                # unknown, closed
                for t1,t2 in self.refSlots:
                    del self.refSlots[t1]
                self.out2.close()
            
        
    async def __callRoutine(self,req:PxpRequest):
        result=await req.callable.call(req)
        req.result=result
        self.refSlots[req.destAddr]=result
        await self.writeLock.acquire()
        try:
            self.out2.write(req.session)
            await req.callable.writeResult(req)
        finally:
            self.writeLock.release()
        
        
import inspect

class PyCallableWrap(PxpCallable):
    
    def __init__(self,c:typing.Callable):
        self._argsInfo:inspect.FullArgSpec=inspect.getfullargspec(c)
        self.argsType=[]
        self.callable=c
        if 'return' in self._argsInfo.annotations:
            self.retType=self._argsInfo.annotations['return']
        else:
            self.retType=type(None)

        if hasattr(self._argsInfo,'args'):
            for argName in self._argsInfo.args:
                self.argsType.append(self._argsInfo.annotations[argName])
            
        
    async def readParameter(self,req:PxpRequest):
        req.parameter=[]
        for t1 in self.argsType:
            if t1==int:
                req.parameter.append(
                    struct.unpack('<q',await req.context.in2.read(8)))[0]
            elif t1==float:
                req.parameter.append(
                    struct.unpack('<d',await req.context.in2.read(8)))[0]
            elif t1==bool:
                req.parameter.append(
                    bool(struct.unpack('<I',await req.context.in2.read(4))))[0]
            else:
                t2=struct.unpack('<i',await req.context.in2.read(4))[0]
                t3=req.context.refSlots[t2]
                if t1==str and type(t3)==bytes:
                    t3=t3.decode('utf-8')
                req.parameter.append(t3)
    
    async def call(self,req:PxpRequest):
        try:
            r=await self.callable(*req.parameter)
            return r
        except Exception as ex2:
            return ex2
    
    async def writeResult(self,req:PxpRequest):
        t1=type(req.result)
        if t1==int:
            req.context.out2.write(struct.pack('<q',req.result))
        elif t1==float:
            req.context.out2.write(struct.pack('<d',req.result))
        elif t1==bool:
            req.context.out2.write(struct.pack('<i',req.result))
        elif issubclass(t1,Exception):
            req.context.out2.write(struct.pack('<i',1))
        else:
            req.context.out2.write(struct.pack('<i',0))
        



def fillFuncMapBuiltIn(funcMap:typing.Dict):
    async def fn(obj:object)->str:
        return str(obj)
    funcMap['builtin.anyToString']=fn

    async def fn(obj:object)->str:
        if isinstance(obj,Exception):
            return str(obj)
        else:
            return ''
    funcMap['builtin.checkException']=fn

    async def fn(obj:typing)->int:
        return len(obj)
    funcMap['builtin.listElemAt']=fn

    async def fn(obj:typing.List,elem:object)->object:
        return obj.append(elem)
    funcMap['builtin.listAdd']=fn

    async def fn(obj:typing.List,index:int)->object:
        return obj.pop(index)
    funcMap['builtin.listRemove']=fn

    async def fn(obj:typing.List,sep:str):
        return sep.join(obj)
    funcMap['builtin.listStringJoin']=fn
