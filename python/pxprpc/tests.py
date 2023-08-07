


import traceback
import pxprpc.backend
import pxprpc.client
import pxprpc.server

import asyncio

import struct

import logging

logging.basicConfig(level=logging.INFO)


EnableWebsocketServer=False
EnableCSharpClient=False

funcMap=dict()

async def testClient(rpcconn:pxprpc.client.RpcConnection,name:str='default'):
    
    
    client2=pxprpc.client.RpcExtendClient1(rpcconn)
    print(await rpcconn.getInfo())
    get1234=await client2.getFunc('test1.get1234')
    get1234.signature('->o')
    printString=await client2.getFunc('test1.printString')
    printString.signature('o->')
    wait1Sec=await client2.getFunc('test1.wait1Sec')
    wait1Sec.signature('->')
    print('expect 1234')
    t1=await get1234()
    await printString(t1)
    printString.signature('s->')
    await printString('change signature test')
    get1234.signature('->s')
    print('expect client get:1234')
    t1=await get1234()
    print('client get:'+t1)
    await wait1Sec()
    raiseError1=await client2.getFunc('test1.raiseError1')
    raiseError1.signature('->s')
    try:
        print('expect dummy io error')
        t1=await raiseError1()
    except Exception as ex1:
        print('exception catched: '+str(ex1))
    print(name+' test done')

async def amain():
    class test1:
        async def get1234(self)->str:
            return '1234'
        async def printString(self,s:str):
            print(s)
    
    funcMap['test1']=test1()
    
    async def fn():
        await asyncio.sleep(1)
    funcMap['test1.wait1Sec']=fn
    async def fn():
        raise IOError('dummy io error')
    funcMap['test1.raiseError1']=fn
    
    server1=pxprpc.backend.TcpServer('127.0.0.1',1344)
    server1.funcMap=funcMap
    await server1.start()

    client1=pxprpc.backend.TcpClient('127.0.0.1',1344)
    await client1.start()

    await testClient(client1.rpcconn,'python local pxprpc')

    if EnableCSharpClient:
        await cstestmain()
    
    if EnableWebsocketServer:
        await wstunnel4test()
    else:
        await server1.stop()


async def wstunnel4test():
    from aiohttp import web_app
    from aiohttp import web
    from aiohttp import web_request
    from aiohttp import web_ws
    from aiohttp import http_websocket
    from aiohttp import web_runner
    from pxprpc.backend import ServerWebsocketTransport,StreamsFromTransport

    async def wshandler(req:web_request.Request):
        r,w=StreamsFromTransport(ServerWebsocketTransport(req))
        srv=pxprpc.server.ServerContext()
        srv.funcMap.update(funcMap)
        srv.backend1(r,w)
        await srv.serve()
        return web.Response()

    async def wshandlerClient(req:web_request.Request):
        r,w=StreamsFromTransport(ServerWebsocketTransport(req))
        client1=pxprpc.client.RpcConnection()
        client1.backend1(r,w)
        asyncio.create_task(client1.run())
        await testClient(client1,'websocketClient')
        return web.Response()
    
    app1=web_app.Application()
    app1.router.add_route('*','/pxprpc',wshandler)
    app1.router.add_route('*','/pxprpcClient',wshandlerClient)
    await web._run_app(app1,host='127.0.0.1',port=1345)

async def ctestmain():
    client1=pxprpc.backend.TcpClient('127.0.0.1',1089)
    await client1.start()
    print('start client')
    client2=pxprpc.client.RpcExtendClient1(client1.rpcconn)
    t1=await client2.getFunc('printString')
    print('printString:',t1.value)
    t1.signature('b->b')
    print(await t1(b'12345')) 
    t1=await client2.getFunc('printStringUnderline')
    print('printStringUnderline:',t1.value)
    t1.signature('b->b')
    print(await t1(b'45678'))
    
async def cstestmain():
    client1=pxprpc.backend.TcpClient('127.0.0.1',2050)
    await client1.start()
    print('start client')
    await testClient(client1.rpcconn,'c# test')


import sys
if __name__=='__main__':
    if 'enable-websocket-server' in sys.argv:
        EnableWebsocketServer=True
    if 'enable-csharp-client' in sys.argv:
        EnableCSharpClient=True
    asyncio.run(amain())