
import { WebSocketIo } from "./backend";
import { Client, Serializer, Server } from "./base";
import { RpcExtendClient1, RpcExtendClientObject, RpcExtendError, RpcExtendServer1, RpcExtendServerCallable, TableSerializer } from "./extend";



export async function testAsClient(){
    let client2=await new RpcExtendClient1(new Client(
        await new WebSocketIo().connect('ws://127.0.0.1:1345/pxprpc'))).init();
    console.log(await client2.conn.getInfo());
    let get1234=(await client2.getFunc('test1.get1234'))!
    get1234.typedecl('->o');
    let str1=await get1234.call() as RpcExtendClientObject;
    let printString=(await client2.getFunc('test1.printString'))!;
    printString.typedecl('o->')
    await printString.call(str1);
    await str1.free()
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
    serdata=new TableSerializer().setHeader('iscl',['id','name','isdir','filesize']).addRow([1554,'1.txt',false,BigInt('12345')]).addRow([1555,'docs',true,BigInt('0')]).build();
    await testTableUnser.call(serdata);
    let raiseError1=(await client2.getFunc('test1.raiseError1'))!
    try{
        raiseError1.typedecl('->s')
        await raiseError1.call()
    }catch(e){
        console.log(e)
    }
}

export async function testAsServer(){
    let server2=await new RpcExtendServer1(new Server(
        await new WebSocketIo().connect('ws://127.0.0.1:1345/pxprpcClient')));
    server2.addFunc('test1.get1234',new RpcExtendServerCallable(async()=>'1234').typedecl('->o'))
    server2.addFunc('test1.printString',new RpcExtendServerCallable(async(s:string)=>console.log(s)).typedecl('o->'))
    server2.addFunc('test1.testUnser',new RpcExtendServerCallable(async(b:ArrayBuffer)=>{
        let ser=new Serializer().prepareUnserializing(b);
        console.log(ser.getInt(),ser.getLong(),ser.getFloat(),ser.getDouble(),ser.getString(),new TextDecoder().decode(ser.getBytes()))
    }).typedecl('b->'))
    server2.addFunc('test1.testTableUnser',new RpcExtendServerCallable(async(b:ArrayBuffer)=>{
        let ser=new TableSerializer().load(b);
        console.log(ser.headerName!.join('\t'))
        for(let i1=0;i1<ser.getRowCount();i1++){
            console.log(ser.getRow(i1).join('\t'));
        }
    }).typedecl('b->'))
    server2.addFunc('test1.wait1Sec',new RpcExtendServerCallable(
        ()=>new Promise((resolve)=>setTimeout(resolve,1000))).typedecl('->'));
    server2.addFunc('test1.raiseError1',new RpcExtendServerCallable(
        async ()=>{throw new Error('dummy io error')}).typedecl('->'));
    server2.addFunc('test1.testPrintArg',new RpcExtendServerCallable(
        async (a,b,c,d,e,f)=>{console.log(a,b,c,d,e,new TextDecoder().decode(f));return [100,BigInt('1234567890')]}).typedecl('cilfdb->il'));
    await server2.serve();
}
