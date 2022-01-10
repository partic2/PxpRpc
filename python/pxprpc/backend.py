


import asyncio
import typing
import traceback

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
                    ctx.backend1(r,w)
                    ctx.funcMap.update(self.funcMap)
                    await ctx.handle()
                finally:
                    self.ctxs.remove(ctx)
            finally:
                w.close()
        
        async def start(self):
            self.srv1:asyncio.AbstractServer=await asyncio.start_server(self.__newConnectionHandler,self.host,self.port)
            
        async def stop(self):
            for t1 in self.ctxs:
                t1.running=False
                t1.out2.close()
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
            self.rpcconn.backend1(r,w)
            self.runtask=asyncio.create_task(self.rpcconn.run())
            self.__running=True

        async def stop(self):
            if self.__running:
                self.rpcconn.running=False
                self.runtask.cancel()
                self.__running=False
                self.rpcconn.close()

except ImportError:
    traceback.print_exc()
    pass
