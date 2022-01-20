
import asyncio
import logging
import struct
import random
import typing
import traceback

log1=logging.getLogger(__name__)

class RpcConnection(object):
    
    def __init__(self):
        self.in2:asyncio.StreamReader=None
        self.out2:asyncio.StreamWriter=None
        self.__waitingSession:typing.Dict[int,asyncio.Future]=dict()
        self.__nextSid=0
        self.__readingResp=None
        self.__writeLock=asyncio.Lock()
    
    def backend1(self,r:asyncio.StreamReader,w:asyncio.StreamWriter):
        self.in2=r
        self.out2=w
        
    async def run(self):
        self.running=True
        try:
            while self.running:
                sid=struct.unpack('<I',await self.in2.readexactly(4))[0]
                log1.debug('client get sid:%s',sid)
                fut=self.__waitingSession[sid]
                del self.__waitingSession[sid]
                self.__readingResp=asyncio.Future()
                fut.set_result(None)
                await self.__readingResp
        except Exception:
            log1.debug('client error:%s',traceback.format_exc())
            self.running=False

    
    async def __newSession(self,opcode:int)->int:
        sid=self.__nextSid
        self.__nextSid+=0x100
        return sid|opcode
        
    async def push(self,destAddr:int,data:bytes):
        sid=await self.__newSession(1)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,len(data)))
            self.out2.write(data)
        finally:
            self.__writeLock.release()
        await respFut
        self.__readingResp.set_result(None)
    
    async def pull(self,srcAddr:int)->bytes:
        sid=await self.__newSession(2)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<II',sid,srcAddr))
        finally:
            self.__writeLock.release()
        await respFut
        size=struct.unpack('<i',await self.in2.readexactly(4))[0]
        data=None
        if size!=-1:
            data=await self.in2.readexactly(size)
        self.__readingResp.set_result(None)
        return data

    async def assign(self,destAddr:int,srcAddr:int):
        sid=await self.__newSession(3)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,srcAddr))
        finally:
            self.__writeLock.release()
        await respFut
        self.__readingResp.set_result(None)

    async def unlink(self,destAddr:int,srcAddr:int):
        sid=await self.__newSession(4)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<II',sid,destAddr))
        finally:
            self.__writeLock.release()
        await respFut
        self.__readingResp.set_result(None)

    async def call(self,destAddr:int,fnAddr:int,argsData:bytes,returnLength:int=4):
        sid=await self.__newSession(5)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,fnAddr))
            self.out2.write(argsData)
        finally:
            self.__writeLock.release()
        await respFut
        data=await self.in2.readexactly(returnLength)
        self.__readingResp.set_result(None)
        return data

    async def getFunc(self,destAddr:int,fnName:int):
        sid=await self.__newSession(6)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,fnName))
        finally:
            self.__writeLock.release()
        await respFut
        t1=struct.unpack('<I',await self.in2.readexactly(4))[0]
        self.__readingResp.set_result(None)
        return t1

    async def getInfo(self):
        sid=await self.__newSession(8)
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<I',sid))
        finally:
            self.__writeLock.release()
        await respFut
        size=struct.unpack('<i',await self.in2.readexactly(4))[0]
        data=None
        if size!=-1:
            data=await self.in2.readexactly(size)
        self.__readingResp.set_result(None)
        return data.decode('utf-8')

    async def close(self):
        sid=await self.__newSession(7)
        respFut=asyncio.Future()
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<I',sid))
        finally:
            self.__writeLock.release()
            self.out2.close()


class RpcExtendClientObject():
    def __init__(self,client:'RpcExtendClient1'):
        self.value=None
        self.client=client

    def tryPull(self):
        return self.conn.pull(self.value)

class RpcExtendClientCallable(RpcExtendClientObject):
    def __init__(self,client):
        super().__init__(client)

    def signature(self,sign:str):
        ''' function signature
format: 'parameters type->return type' 
eg:
a function defined in c:
    bool fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
defined in java:
    boolean fn(int,int,double,Object)
    ...
it's pxprpc signature: 
    iido->z

available type signature characters:
  i  int(32bit integer)
  l  long(64bit integer)
  f  float(32bit float)
  d  double(64bit float)
  o  object(32bit reference address)
  f  function(32bit address refer to a function/callable object)
  b  bytes(32bit address refer to a bytes buffer)

  z  boolean(pxprpc use 32bit to store boolean value)
  s  string(bytes will be decode to string)
        '''
        self.signature=sign

    async def __call__(self,*args)->typing.Any:
        sign=self.signature
        t1=0
        fmtstr=''
        args2=[]
        for t1 in range(0,len(sign)):
            if sign[t1]=='l':
                fmtstr+='q'
                args2.append(args[t1])
            elif sign[t1]=='o':
                fmtstr+='i'
                args2.append(args[t1].value)
            elif sign[t1]=='z':
                fmtstr+='i'
                args2.append(args[t1]!=0)
            elif sign[t1] in('s','b'):
                fmtstr+='i'
                args2.append((await self.client.newTempVariable(args[t1])).value)


class RpcExtendClient1:
    def __init__(self,conn:RpcConnection):
        self.conn=conn

    async def newTempVariable(self,value,type:str='auto')->RpcExtendClientObject:
        #TODO: create temporary variable, manager lifecycle
        pass