


import asyncio
import asyncio.exceptions
import typing
import traceback

import logging
log1=logging.getLogger(__name__)

from .common import AbstractIo

class StreamIo(AbstractIo):
    def __init__(self,r:asyncio.StreamReader,w:asyncio.StreamWriter):
        self.r=r
        self.w=w

    async def receive(self)->bytes:
        len=int.from_bytes(await self.r.readexactly(4),'little')
        return await self.r.readexactly(len)
    
    async def send(self,buf:bytes):
        self.w.write(len(buf).to_bytes(4,'little'))
        self.w.write(buf)

    def close(self):
        self.w.close()

try:
    from . import server
    class TcpServer:
        def __init__(self,host:str,port:int):
            self.host=host
            self.port=port
            self.ctxs:typing.List[server.ServerContext]=[]
            self.funcMap=dict()
        
        async def __newConnectionHandler(self,r:asyncio.StreamReader,w:asyncio.StreamWriter):
            try:
                ctx=server.ServerContext()
                self.ctxs.append(ctx)
                try:
                    ctx.backend1(StreamIo(r,w))
                    ctx.funcMap.update(self.funcMap)
                    await ctx.serve()
                except asyncio.exceptions.IncompleteReadError:
                    log1.debug('connection closed')
                except Exception as exc:
                    log1.debug('server exception:%s',traceback.format_exc())
                finally:
                    self.ctxs.remove(ctx)
            finally:
                w.close()
        
        async def start(self):
            self.srv1:asyncio.AbstractServer=await asyncio.start_server(self.__newConnectionHandler,self.host,self.port)
            
        async def stop(self):
            for t1 in self.ctxs:
                t1.running=False
                t1.io1.close()
            self.srv1.close()

except ImportError:
    traceback.print_exc()
    pass

try:
    from . import client
    
    class TcpClient:
        def __init__(self,host:str,port:int):
            self.host=host
            self.port=port
            
        async def start(self):
            r,w=await asyncio.open_connection(self.host,self.port)
            self.rpcconn=client.RpcConnection()
            self.rpcconn.backend1(StreamIo(r,w))
            self.runtask=asyncio.create_task(self.rpcconn.run())
            self.__running=True

        async def stop(self):
            if self.__running:
                self.rpcconn.running=False
                self.runtask.cancel()
                self.__running=False
                await self.rpcconn.close()

except ImportError:
    traceback.print_exc()
    pass


try:
    from aiohttp import web_app
    from aiohttp import web
    from aiohttp import web_request
    from aiohttp import web_ws
    from aiohttp import http_websocket
    from aiohttp import web_runner
    import aiohttp


    class WebSocketServerIo(AbstractIo):
        def __init__(self,req:web_request.Request):
            super().__init__()
            self.req=req
            self.writequeue=asyncio.Queue(0)
            self.ready=asyncio.Future()
            asyncio.create_task(self._prepare())

        async def _prepare(self):
            ws1=web_ws.WebSocketResponse()
            self.ws1=ws1
            await ws1.prepare(self.req)
            self.ready.set_result(None)

        async def receive(self)->bytes:
            await self.ready
            while not self.ws1.closed:
                msg=await self.ws1.receive()
                if msg.type==http_websocket.WSMsgType.BINARY:
                    return msg.data
            raise IOError('websocket closed')
        
        async def send(self,buf:bytes):
            await self.ready
            await self.ws1.send_bytes(buf)

        def close(self):
            asyncio.create_task(self.ws1.close())

    class WebSocketClientIo(AbstractIo):
        def __init__(self,wsconn:aiohttp.ClientWebSocketResponse):
            self.ws1=wsconn

        async def receive(self)->bytes:
            while not self.ws1.closed:
                msg=await self.ws1.receive()
                if msg.type==http_websocket.WSMsgType.BINARY:
                    return msg.data
            raise IOError('websocket closed')
        
        async def send(self,buf:bytes):
            await self.ws1.send_bytes(buf)

        def close(self):
            asyncio.create_task(self.ws1.close())



    class ServerWebsocketTransport(asyncio.Transport):
        
        def __init__(self,req:web_request.Request):
            super().__init__()
            self.req=req
            self.writequeue=asyncio.Queue(0)
            asyncio.create_task(self._serve())

        async def _serve(self):
            ws1=web_ws.WebSocketResponse()
            self.ws1=ws1
            await ws1.prepare(self.req)

            async def receiver():
                self._prot.connection_made(self)
                while not ws1.closed:
                    msg=await ws1.receive()
                    if msg.type==http_websocket.WSMsgType.BINARY:
                        self._prot.data_received(msg.data)
                    elif msg.type==http_websocket.WSMsgType.CLOSE:
                        self._prot.connection_lost(None)
                        break
                    
            async def sender():
                while not ws1.closed:
                    data1=await self.writequeue.get()
                    if data1==None:
                        await ws1.close()
                    await ws1.send_bytes(data1)
            try:
                done,pending=await asyncio.wait([receiver(),sender()],return_when=asyncio.FIRST_COMPLETED)
                for t1 in pending:
                    t1.cancel()
            except Exception as ex:
                import traceback
                traceback.print_exc()

        def is_reading(self):
            return True

        def set_protocol(self, protocol:asyncio.Protocol):
            self._prot=protocol

        def get_protocol(self):
            return self._prot

        def write(self, data) -> None:
            self.writequeue.put_nowait(data)

        def write_eof(self) -> None:
            self.writequeue.put_nowait(None)

        def can_write_eof(self) -> bool:
            return True

        def close(self) -> None:
            asyncio.create_task(self.ws1.close())

    class ClientWebsocketTransport(asyncio.Transport):
                
        def __init__(self,url:str,client:aiohttp.ClientSession):
            super().__init__()
            self.client=client
            self.url=url
            self.writequeue=asyncio.Queue(0)
            asyncio.create_task(self._serve())
            self.funcMap=dict()

        async def _serve(self):
            self.conn=await self.client.ws_connect(self.url)

            async def receiver():
                self._prot.connection_made(self)
                while not self.conn.closed:
                    msg=await self.conn.receive()
                    if msg.type==http_websocket.WSMsgType.BINARY:
                        self._prot.data_received(msg.data)
                    elif msg.type==http_websocket.WSMsgType.CLOSE:
                        self._prot.connection_lost(None)
                        break
                    
            async def sender():
                while not self.conn.closed:
                    data1=await self.writequeue.get()
                    if data1==None:
                        await self.conn.close()
                        break
                    await self.conn.send_bytes(data1)
            try:
                done,pending=await asyncio.wait([receiver(),sender()],return_when=asyncio.FIRST_COMPLETED)
                for t1 in pending:
                    t1.cancel()
            except Exception as ex:
                import traceback
                traceback.print_exc()

        def is_reading(self):
            return True

        def set_protocol(self, protocol:asyncio.Protocol):
            self._prot=protocol

        def get_protocol(self):
            return self._prot

        def write(self, data) -> None:
            self.writequeue.put_nowait(data)

        def write_eof(self) -> None:
            self.writequeue.put_nowait(None)

        def can_write_eof(self) -> bool:
            return True

        def close(self) -> None:
            asyncio.create_task(self.conn.close())


except ImportError:
    pass

def StreamsFromTransport(transport:asyncio.Transport)->typing.Tuple[asyncio.StreamReader,asyncio.StreamWriter]:
    r=asyncio.StreamReader()
    transport.set_protocol(asyncio.StreamReaderProtocol(r))
    w=asyncio.StreamWriter(transport,transport.get_protocol(),r,asyncio.get_event_loop())
    return (r,w)