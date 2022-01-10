
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
        self.__readingResp.set_result(None)

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