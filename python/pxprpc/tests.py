


import pxprpc.backend

import asyncio

import struct

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
    await server1.start()
    await client1.start()

    print(await client1.rpcconn.getInfo())
    await client1.rpcconn.push(1,b'test1.get1234')
    await client1.rpcconn.getFunc(10,1)
    await client1.rpcconn.push(1,b'test1.printString')
    await client1.rpcconn.getFunc(11,1)
    await client1.rpcconn.push(1,b'test1.wait1Sec')
    await client1.rpcconn.getFunc(12,1)
    await client1.rpcconn.call(13,10,b'')
    await client1.rpcconn.call(1,11,struct.pack('<i',13))
    await client1.rpcconn.call(1,12,b'')
    print('done')
    


if __name__=='main':
    asyncio.run(amain())