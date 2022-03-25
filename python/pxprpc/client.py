
import asyncio
from asyncio.tasks import create_task
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
        except Exception as exc:
            log1.debug('client error:%s',repr(exc))
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

    async def unlink(self,destAddr:int):
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
    def __init__(self,client:'RpcExtendClient1',value=None):
        self.value=value
        self.client=client

    def tryPull(self)->bytes:
        return self.client.conn.pull(self.value)

    async def free(self):
        if self.value!=None:
            val=self.value
            self.value=None
            await self.client.freeSlot(val)
            

    async def asCallable(self):
        '''this object will be invalid after this function. use return value instead.'''
        c1=RpcExtendClientCallable(self.client,self.value)
        self.value=None
        return c1
            
    def __del__(self):
        if self.value!=None:
            val=self.value
            self.value=None
            create_task(self.client.freeSlot(val))
            

class RpcExtendClientCallable(RpcExtendClientObject):
        
        
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
  b  bytes(32bit address refer to a bytes buffer)
  v  void(32bit 0)

  z  boolean(pxprpc use 32bit to store boolean value)
  s  string(bytes will be decode to string)
        '''
        self.sign=sign


    async def __call__(self,*args)->typing.Any:
        sign=self.sign
        freeBeforeReturn=[]
        t1=0
        fmtstr=''
        args2=[]
        retType='v'
        try:
            for t1 in range(0,len(sign)):
                if sign[t1]=='l':
                    fmtstr+='q'
                    args2.append(args[t1])
                elif sign[t1]=='o':
                    fmtstr+='i'
                    args2.append(args[t1].value)
                elif sign[t1]=='z':
                    fmtstr+='i'
                    args2.append(1 if args[t1] else 0)
                elif sign[t1] in('b','s'):
                    fmtstr+='i'
                    t2=await self.client.allocSlot()
                    freeBeforeReturn.append(t2)
                    if sign[t1]=='s':
                        await self.client.conn.push(t2,args[t1].encode('utf-8'))
                    else:
                        await self.client.conn.push(t2,args[t1])
                    args2.append(t2)
                elif sign[t1]=='-':
                    if sign[t1+1]=='>':
                        if len(sign)>t1+2:
                            retType=sign[t1+2]
                        break
                elif sign[t1] == 'v':
                    raise RpcExtendError('Unsupport input argument')
                else:
                    fmtstr+=sign[t1]
                    args2.append(args[t1])
            
            packed=struct.pack('<'+fmtstr,*args2) if len(fmtstr)>0 else bytes()

            if retType in 'ilfdvz':
                result=await self.client.conn.call(0,self.value,packed,4 if retType in 'ifvz' else 6)
                if retType=='l':
                    return struct.unpack('<q',result)
                elif retType=='z':
                    return result!=bytes((0,0,0,0))
                elif retType in 'v':
                    return None
                else:
                    return struct.unpack('<'+retType,result)
            else:
                t1=await self.client.allocSlot()
                await self.client.conn.call(t1,self.value,packed,4)
                if retType=='s':
                    t2=(await self.client.conn.pull(t1)).decode('utf8')
                    await self.client.freeSlot(t1)
                    return t2
                elif retType=='b':
                    t2=await self.client.conn.pull(t1)
                    await self.client.freeSlot(t1)
                    return t2
                else:
                    return RpcExtendClientObject(self.client,value=t1)
        finally:
            for t1 in freeBeforeReturn:
                await self.client.freeSlot(t1)




class RpcExtendError(Exception):
    pass

class RpcExtendClient1:
    def __init__(self,conn:RpcConnection):
        self.conn=conn
        self.__usedSlots:typing.Set[int]=set()
        self.__slotStart=1
        self.__slotEnd=64
        self.__nextSlots=self.__slotStart

    async def start(self):
        self.conn.run()

    async def allocSlot(self)->int:
        reachEnd=False
        while self.__nextSlots in self.__usedSlots:
            # interuptable
            await asyncio.sleep(0)
            self.__nextSlots+=1
            if self.__nextSlots>=self.__slotEnd:
                if reachEnd:
                    raise RpcExtendError('No slot available')
                else:
                    reachEnd=True
                    self.__nextSlots=self.__slotStart

        t1=self.__nextSlots
        self.__nextSlots+=1
        if self.__nextSlots>=self.__slotEnd:
            self.__nextSlots=self.__slotStart
        self.__usedSlots.add(t1)
        return t1

    async def freeSlot(self,index:int):
        if self.conn.running:
            await self.conn.unlink(index)
        self.__usedSlots.remove(index)

    async def getFunc(self,name:str)->RpcExtendClientCallable:
        t1=await self.allocSlot()
        await self.conn.push(t1,name.encode('utf8'))
        t2=await self.allocSlot()
        t3=await self.conn.getFunc(t2,t1)
        if t3==0:
            await self.freeSlot(t2)
            await self.freeSlot(t1)
            return None
        else:
            await self.freeSlot(t1)
            return RpcExtendClientCallable(self,value=t2)
