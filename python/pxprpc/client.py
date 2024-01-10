
import asyncio
from asyncio.tasks import create_task
import logging
import struct
import typing
import traceback


from typing import cast,Optional,Any,Tuple

from pxprpc.common import NotNone

log1=logging.getLogger(__name__)

from .common import Serializer,AbstractIo


class RpcRemoteError(Exception):
    pass

class RpcConnection(object):
    
    def __init__(self):
        self.__waitingSession:typing.Dict[int,asyncio.Future[Tuple[int,bytes]]]=dict()
        self.__readingResp=None
        self.__writeLock=asyncio.Lock()
        self.running=False
    
    def backend1(self,io1:AbstractIo):
        self.io1=io1
        
    async def run(self):
        self.running=True
        try:
            while self.running:
                pack=await self.io1.receive()
                sid=struct.unpack('<I',pack[:4])[0]
                log1.debug('client get sid:%s',hex(sid))
                fut=self.__waitingSession[sid&0x7fffffff]
                del self.__waitingSession[sid&0x7fffffff]
                fut.set_result((sid,pack[4:]))
        except Exception as exc:
            for waiting in self.__waitingSession.values():
                waiting.set_exception(exc)
            log1.debug('client error:%s',repr(exc))
        finally:
            self.running=False


    async def call(self,callableIndex:int,parameter:bytes,sid:int=0x100)->bytes:
        respFut=asyncio.Future()
        self.__waitingSession[sid]=respFut
        await self.io1.send(struct.pack('Ii',sid,callableIndex)+parameter)
        sid2,result=await respFut
        if sid==sid2:
            return result
        else:
            raise RpcRemoteError(result.decode('utf-8'))

    async def getFunc(self,fnName:str,sid:int=0x100)->int:
        result=await self.call(-1,fnName.encode('utf-8'),sid)
        return int.from_bytes(result,'little',signed=True)

    async def freeRef(self,index:int,sid:int=0x100):
        await self.call(-2,bytes(index.to_bytes(4,'little',signed=True)),sid)

    async def close(self,sid:int=0x100):
        #no return
        await self.io1.send(struct.pack('Ii',sid,-3))
        self.running=False

    async def getInfo(self,sid:int=0x100):
        result=await self.call(-4,bytes(),sid)
        return result.decode('utf-8')

    async def sequence(self,mask:int,maskCnt:int=24,sid:int=0x100):
        await self.call(-5,(mask|maskCnt).to_bytes(4,'little',signed=False),sid)

class RpcExtendClientObject():
    def __init__(self,client:'RpcExtendClient1',value:typing.Optional[int]=None):
        self.value=value
        self.client=client

    async def free(self):
        if self.value is not None:
            val=self.value
            self.value=None
            await self.client.conn.freeRef(val)
            

    async def asCallable(self):
        '''this object will be invalid after this function. use return value instead.'''
        c1=RpcExtendClientCallable(self.client,self.value)
        self.value=None
        return c1
            
    def __del__(self):
        log1.debug('running:',self.client.conn.running)
        if self.client.conn.running:
            create_task(self.client.freeRef(self))
            
   
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


    async def __call__(self,*args)->typing.Any:
        assert self.value is not None
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
                    if t2==None:
                        ser.putInt(-1)
                    else:
                        ser.putInt(t2.value)
                elif t1=='c':
                    ser.putVarint(1 if t2 else 0)
                elif t1=='s':
                    ser.putString(t2)
                elif t1=='b':
                    ser.putBytes(t2)
                else:
                    raise IOError('Unknown typedecl character')

            packed=ser.build()
        sid=self.client.allocSid()
        result=await self.client.conn.call(self.value,packed,sid)
        self.client.freeSid(sid)
        if self.tResult=='b':
            return result
        else:
            ser=Serializer().prepareUnserializing(result)
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
                    val=ser.getInt()
                    if val==-1:
                        results.append(None)
                    else:
                        results.append(RpcExtendClientObject(self.client,val))
                else:
                    assert False,'Unreachable'

            if len(results)==0:
                return None
            elif len(results)==1:
                return results[0]
            else:
                return results




class RpcExtendError(Exception):
    pass

class RpcExtendClient1:
    def __init__(self,conn:RpcConnection):
        self.conn=conn
        self.__usedSid:typing.Set[int]=set()
        self.__sidStart=1
        self.__sidEnd=256
        self.__nextSid=self.__sidStart
        self.builtIn=None

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
        self.__usedSid.remove(index)

    async def freeRef(self,ref:RpcExtendClientObject):
        if(ref.value!=None):
            val=ref.value
            ref.value=None
            await self.conn.freeRef(val)

    async def getFunc(self,name:str)->typing.Optional[RpcExtendClientCallable]:
        index=await self.conn.getFunc(name,self.__sidEnd+1)
        if index==-1:
            return None
        return RpcExtendClientCallable(self,value=index)
