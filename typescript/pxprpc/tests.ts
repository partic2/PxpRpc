
import { WebSocketIo } from "./backend";
import { Client, Serializer, Server } from "./base";
import { RpcExtendClient1, RpcExtendClientObject, RpcExtendServer1, RpcExtendServerCallable, TableSerializer, defaultFuncMap } from "./extend";

/*
//Test code for WebMessage(Worker) backend, require test environent.
import { CreateWorkerThread } from 'partic2/jsutils1/webutils';
import {WebMessage} from 'pxprpc/backend'
import { RpcExtendServer1,RpcExtendClient1 } from 'pxprpc/extend';
import { Server,Client } from 'pxprpc/base';
var __name__='testxxx/index'

;(async ()=>{
    if(globalThis.window!=undefined){
        let workerThread=CreateWorkerThread();
        await workerThread.start();
        WebMessage.bind(workerThread.port!);
        let serv=new WebMessage.Server(async (conn)=>{
            let server2=await new RpcExtendServer1(new Server(conn));
            await testAsServer(server2);
        });
        serv.listen('pxprpc test 1');
        await workerThread.runScript(`require(['${__name__}'])`)
    }else{
        console.log('worker')
        WebMessage.bind(globalThis);
        let client2=await new RpcExtendClient1(new Client(
            await new WebMessage.Connection().connect('pxprpc test 1'))).init();
        await testAsClient(client2);
    }
})();
;(async ()=>{
    if(globalThis.window!=undefined){
        let workerThread=new WorkerThread();
        await workerThread.start();
        WebMessage.bind(workerThread.worker!);
        let serv=new WebMessage.Server(async (conn)=>{
            let server2=await new RpcExtendServer1(new Server(conn));
            await testAsServer(server2);
        });
        serv.listen('pxprpc test 1');
        await workerThread.runScript(`require(['${__name__}'])`)
    }else{
        console.log('worker')
        WebMessage.bind(globalThis);
        let client2=await new RpcExtendClient1(new Client(
            await new WebMessage.Connection().connect('pxprpc test 1'))).init();
        await testAsClient(client2);
    }
})();
*/

export async function testAsClient(client2?:RpcExtendClient1){
    try{
    if(client2==undefined){
        client2=await new RpcExtendClient1(new Client(
            await new WebSocketIo().connect('ws://127.0.0.1:1345/pxprpc'))).init();
    }
    console.log(await client2.conn.getInfo());
    console.log('server name:'+client2.serverName)
    let get1234=(await client2.getFunc('test1.get1234'))!
    get1234.typedecl('->o');
    let str1=await get1234.call() as RpcExtendClientObject;
    let printString=(await client2.getFunc('test1.printString'))!;
    printString.typedecl('o->')
    await printString.call(str1);
    await str1.free()
    let testNone=(await client2.getFunc('test1.testNone'))!
    testNone.typedecl('o->o');
    console.log('expect null:',await testNone.call(null));
    let testPrintArg=(await client2.getFunc('test1.testPrintArg'))!
    testPrintArg.typedecl('cilfdb->il')
    console.log('multi-result:',await testPrintArg.call(true,123,BigInt('1122334455667788'),123.5,123.123,new TextEncoder().encode('bytes')));
    let testUnser=(await client2.getFunc('test1.testUnser'))!;
    testUnser.typedecl('b->')
    let serdata=new Serializer().prepareSerializing(8)
    .putInt(123).putLong(BigInt('1122334455667788')).putFloat(123.5).putDouble(123.123).putString('abcdef').putBytes(new TextEncoder().encode('bytes'))
    .build();
    await testUnser.call(serdata);
    let testTableUnser=(await client2.getFunc('test1.testTableUnser'))!;
    testTableUnser.typedecl('b->')
    serdata=new TableSerializer().setHeader('iscl',null).fromMapArray([{id:1554,name:'1.txt',isdir:false,filesize:BigInt('12345')},{id:1555,name:'docs',isdir:true,filesize:BigInt('0')}]).build();
    await testTableUnser.call(serdata);
    let raiseError1=(await client2.getFunc('test1.raiseError1'))!
    try{
        raiseError1.typedecl('->s')
        await raiseError1.call()
    }catch(e){
        console.log(e)
    }
    let autoCloseable=(await client2.getFunc('test1.autoCloseable'))!.typedecl('->o');
    await autoCloseable.call();
    await client2.close()
    }catch(e){
        console.error(e)
    }
}

export async function testAsServer(server2?:RpcExtendServer1){
    try{
    if(server2==undefined){
        server2=await new RpcExtendServer1(new Server(
            await new WebSocketIo().connect('ws://127.0.0.1:1345/pxprpcClient')));
    }
    defaultFuncMap['test1.get1234']=new RpcExtendServerCallable(async()=>'1234').typedecl('->o')
    defaultFuncMap['test1.printString']=new RpcExtendServerCallable(async(s:string)=>console.log(s)).typedecl('o->')
    defaultFuncMap['test1.testUnser']=new RpcExtendServerCallable(async(b:Uint8Array)=>{
        let ser=new Serializer().prepareUnserializing(b);
        console.log(ser.getInt(),ser.getLong(),ser.getFloat(),ser.getDouble(),ser.getString(),new TextDecoder().decode(ser.getBytes()))
    }).typedecl('b->')
    defaultFuncMap['test1.testTableUnser']=new RpcExtendServerCallable(async(b:Uint8Array)=>{
        console.log(new TableSerializer().load(b).toMapArray())
    }).typedecl('b->')
    defaultFuncMap['test1.wait1Sec']=new RpcExtendServerCallable(
        ()=>new Promise((resolve)=>setTimeout(()=>{resolve('tick')},1000))
        ).typedecl('->s');
    defaultFuncMap['test1.raiseError1']=new RpcExtendServerCallable(
        async ()=>{throw new Error('dummy io error')}
        ).typedecl('->');
    defaultFuncMap['test1.testPrintArg']=new RpcExtendServerCallable(
        async (a,b,c,d,e,f)=>{console.log(a,b,c,d,e,new TextDecoder().decode(f));return [100,BigInt('1234567890')]}
        ).typedecl('cilfdb->il');
    defaultFuncMap['test1.testNone']=new RpcExtendServerCallable(
        async (nullValue)=>{console.log('expect null',nullValue);return null;}
        ).typedecl('o->o');
    defaultFuncMap['test1.autoCloseable']=new RpcExtendServerCallable(
        async ()=>{return {close:()=>{console.log('auto closeable closed')}}}
        ).typedecl('->o');
    await server2.serve();
    }catch(e){
        console.error(e)
    }
}
