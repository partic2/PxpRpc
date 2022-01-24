import { PxprpcWebSocketClient } from "./backend";
import { RpcExtendClient1, RpcExtendClientObject } from "./extend";



export async function test(){
    let client1=new PxprpcWebSocketClient();
    await client1.connect('ws://127.0.0.1:1345/pxprpc')
    client1.run()
    let client2=new RpcExtendClient1(client1.rpc()!);
    console.log(await client2.conn.getInfo());
    let get1234=await client2.getFunc('test1.get1234')
    get1234.signature('->s');
    console.log(await get1234.call())
    get1234.signature('->o');
    let str1=await get1234.call() as RpcExtendClientObject;
    let printString=await client2.getFunc('test1.printString');
    printString.signature('o->')
    await printString.call(str1);
    printString.signature('s->')
    await printString.call('4567');
    await str1.free()
}