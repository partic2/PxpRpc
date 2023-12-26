
import asyncio
from asyncio.tasks import create_task
import logging
import struct
import typing
import traceback

from pxprpc.common import NotNone

log1=logging.getLogger(__name__)

from .common import Serializer

class RpcConnection(object):
    
    def __init__(self):
        self.in2:asyncio.StreamReader=None #type:ignore
        self.out2:asyncio.StreamWriter=None #type:ignore
        self.__waitingSession:typing.Dict[int,asyncio.Future]=dict()
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
                log1.debug('client get sid:%s',hex(sid))
                fut=self.__waitingSession[sid]
                del self.__waitingSession[sid]
                self.__readingResp=asyncio.Future()
                fut.set_result(None)
                await self.__readingResp
        except Exception as exc:
            for waiting in self.__waitingSession.values():
                waiting.set_exception(exc)
            log1.debug('client error:%s',repr(exc))
            self.running=False
        
    async def push(self,destAddr:int,data:bytes,sid:int=0x100):
        sid=sid|1
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,len(data)))
            self.out2.write(data)
        finally:
            self.__writeLock.release()
        await respFut
        NotNone(self.__readingResp).set_result(None)
    
    async def pull(self,srcAddr:int,sid:int=0x100)->bytes:
        sid=sid|2
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
        assert size!=-1,'data do NOT support pull'
        data=await self.in2.readexactly(size)
        NotNone(self.__readingResp).set_result(None)
        return data

    async def assign(self,destAddr:int,srcAddr:int,sid:int=0x100):
        sid=sid|3
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,srcAddr))
        finally:
            self.__writeLock.release()
        await respFut
        NotNone(self.__readingResp).set_result(None)

    async def unlink(self,destAddr:int,sid:int=0x100):
        sid=sid|4
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<II',sid,destAddr))
        finally:
            self.__writeLock.release()
        await respFut
        NotNone(self.__readingResp).set_result(None)

    async def call(self,destAddr:int,fnAddr:int,argsData:typing.List[bytes],onResponse:typing.Callable[[asyncio.StreamReader],typing.Awaitable[typing.Any]],sid:int=0x100):
        sid=sid|5
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,fnAddr))
            for buf in argsData:
                self.out2.write(buf)
        finally:
            self.__writeLock.release()
        await respFut
        if onResponse!=None:
            await onResponse(self.in2)
        NotNone(self.__readingResp).set_result(None)

    async def getFunc(self,destAddr:int,fnName:int,sid:int=0x100):
        sid=sid|6
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<III',sid,destAddr,fnName))
        finally:
            self.__writeLock.release()
        await respFut
        t1=struct.unpack('<I',await self.in2.readexactly(4))[0]
        NotNone(self.__readingResp).set_result(None)
        return t1

    async def getInfo(self,sid:int=0x100):
        sid=sid|8
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
        assert size!=-1,'get info return nothing'
        data=await self.in2.readexactly(size)
        NotNone(self.__readingResp).set_result(None)
        return data.decode('utf-8')

    async def close(self,sid:int=0x100):
        sid=sid|7
        respFut=asyncio.Future()
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<I',sid))
        finally:
            self.__writeLock.release()
            self.out2.close()

    async def sequence(self,mask:int,maskCnt:int=24,sid:int=0x100):
        sid=sid|9
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<II',sid,mask|maskCnt))
        finally:
            self.__writeLock.release()
        await respFut
        NotNone(self.__readingResp).set_result(None)

    async def buffer(self,sid:int=0x100):
        sid=sid|10
        respFut=asyncio.Future()
        await self.__writeLock.acquire()
        try:
            self.out2.write(struct.pack('<I',sid))
        finally:
            self.__writeLock.release()

class RpcExtendClientObject():
    def __init__(self,client:'RpcExtendClient1',value:typing.Optional[int]=None):
        self.value=value
        self.client=client

    async def tryPull(self)->bytes:
        assert self.value is not None
        return await self.client.conn.pull(self.value)

    async def free(self):
        if self.value is not None:
            val=self.value
            self.value=None
            await self.client.freeSlot(val)
            

    async def asCallable(self):
        '''this object will be invalid after this function. use return value instead.'''
        c1=RpcExtendClientCallable(self.client,self.value)
        self.value=None
        return c1
            
    def __del__(self):
        if self.value is not None:
            val=self.value
            self.value=None
            try:
                create_task(self.client.freeSlot(val))
            except Exception:
                pass
            
   
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
  b  bytes(bytes buffer)

  c  boolean(pxprpc use 1byte(1/0) to store a boolean value)
  s  string(bytes will be decode to string)
        '''
        self.tParam,self.tResult=sign.split('->')
        return self


    async def __call__(self,*args)->typing.Any:
        assert self.value is not None
        try:
            if self.tParam=='b':
                packed=args[0]
            else:
                ser=Serializer().prepareSerializing()
                for t1,t2 in zip(self.tParam,args):
                    if t1=='i':
                        ser.putInt(t2)
                    elif t1=='l':
                        ser.putLong(t2)
                    elif t1=='f':
                        ser.putFloat(t2)
                    elif t1=='d':
                        ser.putDouble(t2)
                    elif t1=='o':
                        ser.putInt(t2.value)
                    elif t1=='c':
                        ser.putVarint(1 if t2 else 0)
                    elif t1=='s':
                        ser.putString(t2)
                    elif t1=='b':
                        ser.putBytes(t2)
                    else:
                        raise IOError('Unknown sign character')

                packed=ser.build()
            destAddr=0
            if self.tResult=='o':
                destAddr=await self.client.allocSlot()

            getResp=asyncio.Future()
            async def onResponse(in2:asyncio.StreamReader):
                len=int.from_bytes(await in2.readexactly(4),'little')
                if (len&0x80000000)!=0:
                    getResp.set_result([False,await in2.readexactly(len&0x7fffffff)])
                else:
                    getResp.set_result([True,await in2.readexactly(len)])

            await self.client.conn.call(destAddr,self.value,[len(packed).to_bytes(4,'little'),packed],onResponse)

            succ,data=await getResp
            
            if succ:
                if self.tResult=='b':
                    return data
                else:
                    ser=Serializer().prepareUnserializing(data)
                    results=[]
                    for t1 in self.tResult:
                        if t1=='i':
                            results.append(ser.getInt())
                        elif t1=='l':
                            results.append(ser.getLong())
                        elif t1=='f':
                            results.append(ser.getFloat())
                        elif t1=='d':
                            results.append(ser.getDouble())
                        elif t1=='b':
                            results.append(ser.getBytes())
                        elif t1=='s':
                            results.append(ser.getString())
                        elif t1=='c':
                            results.append(ser.getVarint()!=0)
                        elif t1=='o':
                            results.append(RpcExtendClientObject(self.client,ser.getInt()))
                        else:
                            raise IOError('Unknown Type')
                if len(results)==0:
                    return None
                elif len(results)==1:
                    return results[0]
                else:
                    return results
            else:
                raise RpcExtendError(ser.getString())
        finally:
            pass




class RpcExtendError(Exception):
    pass

class RpcExtendClient1:
    def __init__(self,conn:RpcConnection):
        self.conn=conn
        self.__usedSlots:typing.Set[int]=set()
        self.__slotStart=1
        self.__slotEnd=64
        self.__nextSlots=self.__slotStart
        self.builtIn=None

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

    async def getFunc(self,name:str)->typing.Optional[RpcExtendClientCallable]:
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

    async def ensureBuiltIn(self):
        if(self.builtIn==None):
            self.builtIn=dict()
            t1=await self.getFunc('builtin.checkException')
            if(t1!=None):
                t1.signature('o->s')
                self.builtIn['checkException']=t1
            
    
    async def checkException(self,obj:RpcExtendClientObject):
        await self.ensureBuiltIn()
        if 'checkException' in NotNone(self.builtIn):
            err=await NotNone(self.builtIn)['checkException'](obj)
            if(err!=''):
                raise RpcExtendError(err)
            
    