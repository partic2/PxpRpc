
import { WebSocketIo } from "./backend";
import { Client } from "./base";
import { RpcExtendClient1, RpcExtendClientObject, RpcExtendError } from "./extend";



export async function test(){
    let client2=await new RpcExtendClient1(new Client(
        await new WebSocketIo().connect('ws://127.0.0.1:1345'))).init();
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