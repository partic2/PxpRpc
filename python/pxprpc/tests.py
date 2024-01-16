


import traceback
import pxprpc.backend
import pxprpc.client
import pxprpc.server
from pxprpc.common import Serializer,TableSerializer

import asyncio

import struct

import logging

logging.basicConfig(level=logging.INFO)

import typing

EnableWebsocketServer=False
EnableCSharpClient=False
EnableCClient=False
EnableJavaClient=False

funcMap=dict()

async def testClient(rpcconn:pxprpc.client.RpcConnection,name:str='default'):
    print('================'+name+'====================')
    
    client2=pxprpc.client.RpcExtendClient1(rpcconn)
    print(await rpcconn.getInfo())
    get1234=await client2.getFunc('test1.get1234')
    assert get1234!=None
    print('get1234:',get1234.value)
    get1234.typedecl('->o')
    printString=await client2.getFunc('test1.printString')
    assert printString!=None
    print('printString:',printString.value)
    printString.typedecl('o->')
    wait1Sec=await client2.getFunc('test1.wait1Sec')
    assert wait1Sec!=None
    print('test1.wait1Sec:',wait1Sec.value)
    wait1Sec.typedecl('->s')
    print('expect 1234')
    t1=await get1234()
    await printString(t1)

    del get1234
    await asyncio.sleep(0.2)
    testNone=await client2.getFunc('test1.testNone')
    assert testNone!=None
    print('testNone:',testNone.value)
    testNone.typedecl('o->o')
    print('expect None:',await testNone(None))
    testPrintArg=await client2.getFunc('test1.testPrintArg')
    assert testPrintArg!=None
    print('testPrintArg:',testPrintArg.value)
    assert testPrintArg!=None
    testPrintArg.typedecl('cilfdb->il')
    print('multi-result:',await testPrintArg(True,123,1122334455667788,123.5,123.123,b'bytes'))
    
    testUnser=await client2.getFunc('test1.testUnser')
    assert testUnser!=None
    print('testUnser:',testUnser.value)
    testUnser.typedecl('b->')
    print('expect 123,1122334455667788,123.5,123.123,abcdef,bytes')
    await testUnser(Serializer().prepareSerializing()\
        .putInt(123).putLong(1122334455667788).putFloat(123.5).putDouble(123.123).putString('abcdef').putBytes('bytes'.encode('utf-8'))\
            .build())
    
    testTableUnser=await client2.getFunc('test1.testTableUnser')
    #optional?
    if testTableUnser!=None:
        print('testTableUnser:',testTableUnser.value)
        testTableUnser.typedecl('b->')
        print('expect a table')
        await testTableUnser(TableSerializer().setHeader('iscl',['id','name','isdir','filesize'])\
                            .addRow([1554,'1.txt',False,12345]).addRow([1555,'docs',True,0]).build())

    print('expect wait 1 second')
    print('expect tick:',await wait1Sec())
    raiseError1=await client2.getFunc('test1.raiseError1')
    assert raiseError1!=None
    raiseError1.typedecl('->s')
    try:
        print('expect dummy io error')
        t1=await raiseError1()
    except Exception as ex1:
        print('exception catched: '+str(ex1))

    t1=await client2.getFunc('test1.missingfunc')
    assert t1==None

    t1=await client2.getFunc('test1.autoCloseable')
    assert t1!=None
    t1.typedecl('->o')
    await t1()
    await client2.conn.close()
    print(name+' test done')
    

from pxprpc.server import decorator

async def amain():
    class test1:
        async def get1234(self)->typing.Any:
            return '1234'
        async def printString(self,s:typing.Any):
            print(s)
            
        async def testUnser(self,b:bytes):
            try:
                ser=Serializer().prepareUnserializing(b)
                print(ser.getInt(),ser.getLong(),ser.getFloat(),ser.getDouble(),ser.getString(),
                ser.getBytes().decode('utf-8'),sep=',')
            except Exception as ex:
                traceback.print_exc()
                raise ex
            
        async def testTableUnser(self,b:bytes):
            ser=TableSerializer().load(b)
            print(*ser.headerName,sep='\t')
            len=ser.getRowCount()
            for t1 in range(len):
                print(*ser.getRow(t1),sep='\t')

        @decorator.typedecl('cilfdb->il')
        async def testPrintArg(self,a:bool,b:int,c:int,d:float,e:float,f:bytes):
            print(a,b,c,d,e,f)
            return [100,1234567890]

        async def testNone(self,noneValue:typing.Any)->typing.Any:
            print('expect None:',noneValue)
            return None
        
        async def autoCloseable(self)->typing.Any:
            class Cls(object):
                def close(self):
                    print('auto closable closed')
            return Cls()

                
    
    funcMap['test1']=test1()
    
    async def fn()->str:
        await asyncio.sleep(1)
        return 'tick'
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
    await client1.stop()

    if EnableJavaClient:
        client1=pxprpc.backend.TcpClient('127.0.0.1',1064)
        await client1.start()
        await testClient(client1.rpcconn,'java pxprpc')
        await client1.stop()

    await asyncio.sleep(1)

    if EnableCClient:
        await ctestmain();
    
    if EnableCSharpClient:
        await cstestmain()
    
    if EnableWebsocketServer:
        await test4wstunnel()
    else:
        await server1.stop()

    print('all test done')


async def test4wstunnel():
    from aiohttp import web_app
    from aiohttp import web
    from aiohttp import web_request
    from aiohttp import web_ws
    from aiohttp import http_websocket
    from aiohttp import web_runner
    from pxprpc.backend import WebSocketClientIo,WebSocketServerIo

    async def wshandler(req:web_request.Request):
        srv=pxprpc.server.ServerContext()
        srv.funcMap.update(funcMap)
        srv.backend1(WebSocketServerIo(req))
        await srv.serve()
        return web.Response()

    async def wshandlerClient(req:web_request.Request):
        client1=pxprpc.client.RpcConnection()
        client1.backend1(WebSocketServerIo(req))
        asyncio.create_task(client1.run())
        await testClient(client1,'websocketClient')
        return web.Response()
    
    app1=web_app.Application()
    app1.router.add_route('*','/pxprpc',wshandler)
    app1.router.add_route('*','/pxprpcClient',wshandlerClient)
    await web._run_app(app1,host='127.0.0.1',port=1345)

async def ctestmain():
    print('===============c test main start================')
    client1=pxprpc.backend.TcpClient('127.0.0.1',1089)
    await client1.start()
    print('start client')
    client2=pxprpc.client.RpcExtendClient1(client1.rpcconn)
    print(await client1.rpcconn.getInfo())
    t1=await client2.getFunc('printString')
    assert t1!=None
    print('printString:',t1.value)
    t1.typedecl('s->s')
    print(await t1('12345')) 
    await t1.free()
    await asyncio.sleep(1)
    t1=await client2.getFunc('printSerilizedArgs')
    assert t1!=None
    print('fnPrintSerilizedArgs:',t1.value)
    t1.typedecl('b->')
    await t1(Serializer().prepareSerializing()\
        .putInt(123).putLong(1122334455667788).putFloat(123.5).putDouble(123.123).putString('abcdef').putBytes('bytes'.encode('utf-8'))\
            .build())
    
async def cstestmain():
    print('cs test main start')
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
    if 'enable-c-client' in sys.argv:
        EnableCClient=True
    if 'enable-java-client' in sys.argv:
        EnableJavaClient=True
    asyncio.run(amain())