


import traceback
import pxprpc.backend
import pxprpc.client

import asyncio

import struct

EnableWebsocketServer=False

async def amain():
    server1=pxprpc.backend.TcpServer('127.0.0.1',1344)
    client1=pxprpc.backend.TcpClient('127.0.0.1',1344)
    
    async def fn()->str:
        return '1234'
    server1.funcMap['test1.get1234']=fn
    async def fn(s:str):
        print(s)
    server1.funcMap['test1.printString']=fn
    async def fn():
        await asyncio.sleep(1)
    server1.funcMap['test1.wait1Sec']=fn
    async def fn():
        raise IOError('dummy io error')
    server1.funcMap['test1.raiseError1']=fn
    await server1.start()
    await client1.start()

    client2=pxprpc.client.RpcExtendClient1(client1.rpcconn)
    print(await client1.rpcconn.getInfo())
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
        print('expected dummy io error')
        t1=await raiseError1()
    except Exception as ex1:
        print('exception catched: '+str(ex1))
    print('done')
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
    async def wshandler(req:web_request.Request):
        ws1=web_ws.WebSocketResponse()
        await ws1.prepare(req)
        r,w=await asyncio.open_connection('127.0.0.1',1344)
        async def receiver():
            while True:
                data1=await ws1.receive()
                if data1.type==http_websocket.WSMsgType.BINARY:
                    w.write(data1.data)
                elif data1.type==http_websocket.WSMsgType.CLOSE:
                    break
                
        async def sender():
            while not r.at_eof():
                data1=await r.read(0x400)
                if len(data1)==0:
                    break
                await ws1.send_bytes(data1)
        try:
            done,pending=await asyncio.wait([receiver(),sender()],return_when=asyncio.FIRST_COMPLETED)
            for t1 in pending:
                t1.cancel()
        except Exception as ex:
            import traceback
            traceback.print_exc()
    app1=web_app.Application()
    app1.router.add_route('*','/pxprpc',wshandler)
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
    client2=pxprpc.client.RpcExtendClient1(client1.rpcconn)
    t1=await client2.getFunc('test.printString')
    print('printString:',t1.value)
    t1.signature('b->b')
    print(await t1(b'12345')) 
    t1=await client2.getFunc('test.printStringUnderline')
    print('printStringUnderline:',t1.value)
    t1.signature('b->b')
    print(await t1(b'45678'))


import sys
if __name__=='__main__':
    if 'enable-websocket-server' in sys.argv:
        EnableWebsocketServer=True
    asyncio.run(amain())