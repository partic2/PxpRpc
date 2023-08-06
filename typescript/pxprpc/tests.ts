
import { WebSocketIo } from "./backend";
import { Client, Server } from "./base";
import { RpcExtendClient1, RpcExtendClientObject, RpcExtendError, RpcExtendServer1, RpcExtendServerCallable } from "./extend";



export async function testAsClient(){
    let client2=await new RpcExtendClient1(new Client(
        await new WebSocketIo().connect('ws://127.0.0.1:1345/pxprpc'))).init();
    console.log(await client2.conn.getInfo());
    let get1234=(await client2.getFunc('test1.get1234'))!
    get1234.signature('->s');
    console.log(await get1234.call())
    get1234.signature('->o');
    let str1=await get1234.call() as RpcExtendClientObject;
    let printString=(await client2.getFunc('test1.printString'))!;
    printString.signature('o->')
    await printString.call(str1);
    printString.signature('s->')
    await printString.call('4567');
    await str1.free()
    let raiseError1=(await client2.getFunc('test1.raiseError1'))!
    try{
        raiseError1.signature('->s')
        await raiseError1.call()
    }catch(e){
        console.log(e)
    }
}

export async function testAsServer(){
    let server2=await new RpcExtendServer1(new Server(
        await new WebSocketIo().connect('ws://127.0.0.1:1345/pxprpcClient')));
    server2.addFunc('test1.get1234',new RpcExtendServerCallable(async()=>'1234').signature('->s'))
    server2.addFunc('test1.printString',new RpcExtendServerCallable(async(s:string)=>console.log(s)).signature('s->'))
    server2.addFunc('test1.wait1Sec',new RpcExtendServerCallable(
        ()=>new Promise((resolve)=>setTimeout(resolve,1000))).signature('->'));
    server2.addFunc('test1.raiseError1',new RpcExtendServerCallable(
        async ()=>{throw new Error('dummy io error')}).signature('->'));
    await server2.serve();
}
