

from typing import Optional,Any,Callable,List,cast,TypeVar,Dict,Tuple
import logging
log1=logging.getLogger(__name__)
import asyncio
from dataclasses import dataclass
import struct

class AbstractIo:
    async def receive(self)->bytes:
        raise NotImplemented()
    
    async def send(self,buf:bytes):
        raise NotImplemented()
    
    def close(self):
        raise NotImplemented()
    

    
T=TypeVar('T')
def NotNone(t:Optional[T])->T:
    return t; #type:ignore

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

class RpcRemoteError(Exception):
    pass

class ClientContext(object):
    
    def __init__(self):
        self.__waitingSession:Dict[int,asyncio.Future[Tuple[int,bytes]]]=dict()
        self.running=False
    
    def backend1(self,io1:AbstractIo):
        self.io1=io1
        
    async def run(self):
        if self.running:
            return
        self.running=True
        try:
            while self.running:
                pack=await self.io1.receive()
                sid=struct.unpack('<I',pack[:4])[0]
                log1.debug('client get sid:%s',hex(sid))
                fut=self.__waitingSession[sid&0x7fffffff]
                del self.__waitingSession[sid&0x7fffffff]
                if not fut.done():
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

    async def freeRef(self,index:List[int],sid:int=0x100):
        await self.call(-2,b''.join(map(lambda t1:t1.to_bytes(4,'little',signed=True),index)),sid)

    async def close(self,sid:int=0x100):
        #no return
        await self.io1.send(struct.pack('Ii',sid,-3))
        self.running=False

    async def getInfo(self,sid:int=0x100):
        result=await self.call(-4,bytes(),sid)
        return result.decode('utf-8')

    async def sequence(self,mask:int,maskCnt:int=24,sid:int=0x100):
        await self.call(-5,(mask|maskCnt).to_bytes(4,'little',signed=False),sid)



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
        self.onFree:Optional[Callable]=None
        self.nextFree:Optional[PxpRef]=None
        self.index=index


import logging

log1=logging.getLogger(__name__)

RefPoolExpandCount=256

DefaultFuncMap:Callable[[str],Optional[PxpCallable]]=lambda x:None

class ServerContext(object):

    
    def __init__(self):
        self.refPool:List[PxpRef]=[]
        self.freeRefEntry:Optional[PxpRef]=None
        self.funcMap=DefaultFuncMap
        self.running=False
        self.pendingRequests:Dict[int,PxpRequest]
        self.sequenceSession:int=0xffffffff
        self.sequenceMaskBitsCnt:int=0


    def expandRefPools(self):
        start=len(self.refPool)
        end=len(self.refPool)+RefPoolExpandCount
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
        t1=self.funcMap(funcName)
        req.result=(-1).to_bytes(4,'little',signed=True)
        if t1!=None:
            ref2=self.allocRef()
            ref2.object=t1
            req.result=ref2.index.to_bytes(4,'little')

    def close(self):
        if not self.running:
            return
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
        try:
            handler=[None,self.getFunc,self.freeRefHandler,self.closeHandler,self.getInfo,self.sequence]
            r2:Optional[PxpRequest]=req
            while r2!=None:
                log1.debug('processing:%s,%s',req.session,req.callableIndex)
                if r2.callableIndex>=0:
                    await cast(PxpCallable,self.getRef(req.callableIndex).object).call(req)
                else:
                    await handler[-r2.callableIndex](r2)
                #abort on close
                if r2.callableIndex==-3:
                    return
                if req.rejected!=None:
                    await self.io1.send((req.session^0x80000000).to_bytes(4,'little')+str(req.rejected).encode('utf-8'))
                else:
                    await self.io1.send(req.session.to_bytes(4,'little')+req.result)
                r2=self.finishRequest(r2)
        except Exception:
            pass
        

    async def serve(self):
        if self.running:
            return
        self.running=True
        try:
            while(self.running):
                req=PxpRequest(self)
                buf=await self.io1.receive()
                req.session,req.callableIndex=struct.unpack('<Ii',buf[:8])
                log1.debug('get session:%s',hex(req.session))
                req.parameter=buf[8:]
                self.queueRequest(req)
        finally:
            self.close()
            