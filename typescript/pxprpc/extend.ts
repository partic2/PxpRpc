import { Client, PxpCallable, PxpObject, PxpRequest, Serializer, Server } from './base'



export class RpcExtendError extends Error {
    public remoteException?:RpcExtendClientObject
}

export class RpcExtendClientObject {
    public constructor(public client: RpcExtendClient1, public value: number | undefined) {
    }
    public async tryPull() {
        return this.client.conn.pull(this.value!);
    }
    public async free() {
        if (this.value != undefined) {
            await this.client.freeSlot(this.value);
        }
    }
    public async asCallable():Promise<RpcExtendClientCallable>{
        return new RpcExtendClientCallable(this.client,this.value)
    }
}

export class RpcExtendClientCallable extends RpcExtendClientObject {
    public tParam: string=''
    public tResult:string=''
    public constructor(client: RpcExtendClient1, value: number | undefined) {
        super(client, value)
    }
    /*
function signature
format: 'parameters type->return type' 
eg:
a function defined in c:
bool fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
defined in java:
boolean fn(int,int,double,Object)
...
it's pxprpc signature: 
iido->c

available type signature characters:
i  int(32bit integer)
l  long(64bit integer) 
f  float(32bit float)
d  double(64bit float)
o  object(32bit reference address)
b  bytes(bytes buffer)
'' return void(32bit 0)

c  boolean(pxprpc use 1byte(1/0) to store a boolean value)
s  string(bytes will be decode to string)
*/
    public signature(sign: string) {
        [this.tParam,this.tResult]=sign.split('->');
        return this;
    }

    public async call(...args:any[]) {
        let buf:ArrayBuffer[]=[]
        if(this.tParam==='b'){
            let abuf=args[0] as ArrayBuffer
            let lenbuf=new ArrayBuffer(4);
            new DataView(lenbuf).setInt32(0,abuf.byteLength,true);
            buf=[lenbuf,abuf];
        }else{
            let ser=new Serializer().prepareSerializing(32);
            for (let i1 = 0; i1 < this.tParam.length; i1++) {
                switch (this.tParam.charAt(i1)) {
                    case 'i':
                        ser.putInt(args[i1]);
                        break;
                    case 'l':
                        ser.putLong(args[i1]);
                        break;
                    case 'f':
                        ser.putFloat(args[i1]);
                        break;
                    case 'd':
                        ser.putDouble(args[i1]);
                        break;
                    case 'o':
                        ser.putInt(args[i1].value);
                        break;
                    case 'b':
                        ser.putBytes(args[i1]);
                        break
                    case 's':
                        ser.putString(args[i1]);
                        break;
                    case 'c':
                        ser.putVarint(args[i1]?1:0);
                        break;
                    default:
                        throw new Error('Unknown type')
                }
            }
            let serbuf=ser.build()
            let lenbuf=new ArrayBuffer(4);
            new DataView(lenbuf).setInt32(0,serbuf.byteLength,true);
            buf=[lenbuf,serbuf];
        }
        let destAddr=0;
        if(this.tResult!=='o')destAddr=await this.client.allocSlot()
        let resp:[boolean,ArrayBuffer]|null=null;
        await this.client.conn.call( destAddr, this.value!, buf,async (io1)=>{
            let intbuf=await io1.read(4);
            let len=new DataView(intbuf).getInt32(0,true); 
            if((len&0x80000000)!==0){
                resp=[false,await io1.read(len&0x7fffffff)]
            }else{
                resp=[true,await io1.read(len)]
            }
        });
        if(resp![0]){
            if(this.tResult==='b'){
                return resp![1];
            }else{
                let ser=new Serializer().prepareUnserializing(resp![1]);
                let rets=[]
                for (let i1 = 0; i1 < this.tResult.length; i1++) {
                    switch (this.tResult.charAt(i1)) {
                        case 'i':
                            rets.push(ser.getInt())
                            break;
                        case 'l':
                            rets.push(ser.getLong())
                            break;
                        case 'f':
                            rets.push(ser.getFloat())
                            break;
                        case 'd':
                            rets.push(ser.getDouble())
                            break;
                        case 'o':
                            rets.push(new RpcExtendClientObject(this.client,ser.getInt()))
                            break;
                        case 'b':
                            rets.push(ser.getBytes())
                            break
                        case 's':
                            rets.push(ser.getString())
                            break;
                        case 'c':
                            rets.push(ser.getVarint()!==0)
                            break;
                        default:
                            throw new Error('Unknown type');
                    }
                }
                if(rets.length===1){
                    return rets[0];
                }else if(rets.length===0){
                    return null;
                }else{
                    return rets;
                }
            }
        }else{
            throw new RpcExtendError(new Serializer().prepareUnserializing(resp![1]).getString())
        }

    }

}



export class RpcExtendClient1 {
    private __usedSlots: { [index: number]: boolean | undefined } = {};
    private __nextSlots: number;

    private __slotStart: number = 1;
    private __slotEnd: number = 64;
    
    public constructor(public conn: Client) {
        this.__nextSlots = this.__slotStart;
    }

    public setAvailableSlotsRange(start:number,end:number){
        this.__slotStart=start;
        this.__slotEnd=end;
        this.__nextSlots = this.__slotStart;
    }

    public async init():Promise<this>{
        if(!this.conn.isRunning()){
            this.conn.run();
        }
        let info=await this.conn.getInfo();
        let refSlotsCap=info.split('\n').find(v=>v.startsWith('reference slots capacity:'))
        if(refSlotsCap!=undefined){
            this.setAvailableSlotsRange(1,Number(refSlotsCap.split(':')[1])-1);
        }
        return this;
    }

    protected builtIn?:{}
    public async ensureBuiltIn(){
        if(this.builtIn==undefined){
            this.builtIn={}
        }
    }
    public async allocSlot() {
        let reachEnd = false;
        while (this.__usedSlots[this.__nextSlots]) {
            this.__nextSlots += 1
            if (this.__nextSlots >= this.__slotEnd) {
                if (reachEnd) {
                    throw new RpcExtendError('No slot available')
                } else {
                    reachEnd = true
                    this.__nextSlots = this.__slotStart
                }
            }
        }

        let t1 = this.__nextSlots
        this.__nextSlots += 1
        if(this.__nextSlots>=this.__slotEnd){
            this.__nextSlots=this.__slotStart;
        }
        this.__usedSlots[t1] = true;
        return t1
    }
    public async freeSlot(index: number) {
        if (this.conn.isRunning()) {
            await this.conn.unlink(index)
            delete this.__usedSlots[index]
        }

    }
    public async getFunc(name: string) {
        let t1 = await this.allocSlot()
        await this.conn.push(t1, new TextEncoder().encode(name))
        let t2 = await this.allocSlot()
        let resp=await this.conn.getFunc(t2, t1)
        await this.freeSlot(t1)
        if(resp==0){
            return null;
        }
        return new RpcExtendClientCallable(this, resp)
    }
    public async close(){
        for(let key in this.__usedSlots){
            if(this.__usedSlots[key])
                this.freeSlot(Number(key));
        }
        await this.conn.close()
    }

}


export class RpcExtendServerCallable implements PxpCallable{
    protected tParam:string = '';
    protected tResult:string='';
    public constructor(public wrapped:(...args:any)=>Promise<any>){
    }
    //See RpcExtendClientCallable.signature
    public signature(sign: string) {
        let [tParam,tResult]=sign.split('->');
        this.tParam=tParam;
        this.tResult=tResult;
        return this;
    }
    public async readParameter (req: PxpRequest){
        let buflen=new DataView(await req.context.io1.read(4)).getUint32(0,true);
        let buf=await req.context.io1.read(buflen&0x7fffffff);
        let param=[];
        let obj:any=null;
        if(this.tParam==='b'){
            param=[buf];
        }else{
            let ser=new Serializer().prepareUnserializing(buf);
            for(let i1=0;i1<this.tParam.length;i1++){
                switch(this.tParam.charAt(i1)){
                    case 'i':
                        param.push(ser.getInt());
                        break;
                    case 'l':
                        param.push(ser.getLong());
                        break;
                    case 'f':
                        param.push(ser.getFloat());
                        break;
                    case 'd':
                        param.push(ser.getDouble());
                        break;
                    case 'o':
                        obj=req.context.refSlots[ser.getInt()]!.get();
                        param.push(obj);
                        break;
                    case 'b':
                        param.push(ser.getBytes());
                        break;
                    case 's':
                        param.push(ser.getString());
                        break;
                    case 'c':
                        param.push(ser.getVarint()!==0);
                        break;
                    default:
                        throw new Error('Unsupported value type.');
                }
            }
        }
        req.parameter=param;
    }
    public async call(req: PxpRequest) : Promise<any>{
        try{
            return await this.wrapped.apply(this,req.parameter);
        }catch(e){
            return e;
        }
    }
    public async writeResult(req: PxpRequest){
        let ser=new Serializer().prepareSerializing(8);
        if(req.result instanceof Error){
            let buf=ser.putString((req.result as Error).message).build();
            let buflen=new ArrayBuffer(4);
            new DataView(buflen).setUint32(0,0x80000000|buf.byteLength,true);
            await Promise.all([req.context.io1.write(buflen),req.context.io1.write(buf)]);
        }else if(this.tResult==='b'){
            let buf=req.result;
            let buflen=new ArrayBuffer(4);
            new DataView(buflen).setUint32(0,buf.byteLength,true);
            await Promise.all([req.context.io1.write(buflen),req.context.io1.write(buf)]);
        }else{
            let results=this.tResult.length>=1?req.result:[req.result]
            for(let i=0;i<this.tResult.length;i++){
                switch(this.tResult.charAt(i)){
                    case 'i':
                        ser.putInt(results[i]);
                        break;
                    case 'l':
                        ser.putLong(results[i]);
                        break;
                    case 'f':
                        ser.putFloat(results[i]);
                        break;
                    case 'd':
                        ser.putDouble(results[i]);
                        break;
                    case 'o':
                        ser.putInt(req.destAddr);
                        break;
                    case 'b':
                        ser.putBytes(results[i]);
                        break;
                    case 's':
                        ser.putString(results[i]);
                        break;
                    case 'c':
                        ser.putVarint(results[i]?1:0);
                        break;
                    case '':
                        break;
                    default:
                        throw new Error('Unsupported value type.');
                }
            }
            let buf=ser.build();
            let buflen=new ArrayBuffer(4);
            new DataView(buflen).setUint32(0,buf.byteLength,true);
            await Promise.all([req.context.io1.write(buflen),req.context.io1.write(buf)]);
        }
    }
}
var builtinServerFuncMap:{[k:string]:RpcExtendServerCallable}={
}
export class RpcExtendServer1{
    public constructor(public serv:Server){
        serv.funcMap=(name)=>this.findFunc(name)
    }
    public async serve(){
        await this.serv.serve()
    }
    public extFuncMap={} as {[k:string]:RpcExtendServerCallable};
    public findFunc(name:string){
        return this.extFuncMap[name]??(builtinServerFuncMap[name]);
    }
    public addFunc(name:string,fn:RpcExtendServerCallable){
        this.extFuncMap[name]=fn;
        return this;
    }

}

export class TableSerializer {
    FLAG_NO_HEADER_NAME=1;
    headerName:string[]|null=null;
    headerType:string|null=null;
    rows:any[]=[];
    public setHeader(types:string,names:string[]){
        this.headerName=names;
        this.headerType=types;
        return this;
    }
    public getRow(index:number){
        return this.rows[index];
    }
    public getRowCount() {
        return this.rows.length
    }
    public addRow(row:any[]){
        this.rows.push(row);return this;
    }
    public load(buf:ArrayBuffer){
        let ser=new Serializer().prepareUnserializing(buf);
        let flag=ser.getInt();
        let rowCnt=ser.getInt();
        this.headerType=ser.getString();
        let colCnt=this.headerType.length;
        if((flag & this.FLAG_NO_HEADER_NAME)===0){
            this.headerName=[]
            for(let i1=0;i1<colCnt;i1++){
                this.headerName.push(ser.getString());
            }
        }
        for(let i1=0;i1<rowCnt;i1++){
            this.rows.push(new Array(colCnt));
        }
        for(let i1=0;i1<colCnt;i1++){
            let type=this.headerType.charAt(i1);
            switch(type){
                case 'i':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getInt();
                    }
                    break;
                case 'l':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getLong();
                    }
                    break;
                case 'f':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getFloat();
                    }
                    break;
                case 'd':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getDouble();
                    }
                    break;
                case 'b':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getBytes();
                    }
                    break;
                case 's':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getString();
                    }
                    break;
                default:
                    throw new Error("Unknown Type");
            }
        }
        return this;
    }


    public build():ArrayBuffer{
        let ser=new Serializer().prepareSerializing(64);
        let i1=0;
        let colsCnt=this.headerType!.length;
        let flag=0;
        if(this.headerName==null){
            flag|=this.FLAG_NO_HEADER_NAME;
        }
        ser.putInt(flag);
        let rowCnt=this.rows.length;
        ser.putInt(rowCnt);
        ser.putString(this.headerType!);
        if(this.headerName!=null){
            for(let e of this.headerName){
                ser.putString(e);
            }
        }
        for(i1=0;i1<colsCnt;i1++){
            let type=this.headerType!.charAt(i1);
            switch(type){
                case 'i':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putInt(this.rows[i2][i1]);
                    }
                    break;
                case 'l':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putLong(this.rows[i2][i1]);
                    }
                    break;
                case 'f':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putFloat(this.rows[i2][i1]);
                    }
                    break;
                case 'd':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putDouble(this.rows[i2][i1]);
                    }
                    break;
                case 'b':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putBytes(this.rows[i2][i1]);
                    }
                    break;
                case 's':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putString(this.rows[i2][i1]);
                    }
                    break;
                default:
                    throw new Error("Unknown Type");
            }
        }
        return ser.build();
    }
}
