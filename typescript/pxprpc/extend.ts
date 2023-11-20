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
    protected sign: string = '->';
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
iido->z

available type signature characters:
i  int(32bit integer)
l  long(64bit integer) 
f  float(32bit float)
d  double(64bit float)
o  object(32bit reference address)
b  bytes(32bit address refer to a bytes buffer)
'' return void(32bit 0)

z  boolean(pxprpc use 32bit to store boolean value)
s  string(bytes will be decode to string)
*/
    public signature(sign: string) {
        this.sign = sign;
        return this;
    }

    public async call(...args:any[]) {
        let sign2 = this.sign
        let argSign = sign2.substring(0, sign2.lastIndexOf('->'))
        let retType = sign2.substring(argSign.length + 2)

        let freeBeforeReturn: number[] = []
        //TODO: fix slightly memory waste.
        let args2 = new DataView(new ArrayBuffer(args.length * 8));
        let writeAt = 0;
        try {
            let argsProcessDone = false;
            for (let i1 = 0; i1 < argSign.length; i1++) {
                switch (argSign.charAt(i1)) {
                    case 'i':
                        args2.setInt32(writeAt, args[i1], true);
                        writeAt += 4;
                        break;
                    case 'l':
                        args2.setBigInt64(writeAt, args[i1], true);
                        writeAt += 8;
                        break;
                    case 'f':
                        args2.setFloat32(writeAt, args[i1], true);
                        writeAt += 4;
                        break;
                    case 'd':
                        args2.setFloat64(writeAt, args[i1], true);
                        writeAt += 8;
                        break;
                    case 'o':
                        args2.setInt32(writeAt, args[i1].value, true);
                        writeAt += 4;
                        break;
                    case 's':
                    case 'b':
                        let t2 = await this.client.allocSlot()
                        freeBeforeReturn.push(t2)
                        if (argSign.charAt(i1) == 's') {
                            await this.client.conn.push(t2, new TextEncoder().encode(args[i1]))
                        } else {
                            await this.client.conn.push(t2, args[i1])
                        }
                        args2.setInt32(writeAt, t2, true);
                        writeAt += 4;
                        break;
                    case 'z':
                        args2.setInt32(writeAt, args[i1] ? 1 : 0, true);
                        writeAt += 4;
                        break;
                }
                if (argsProcessDone) {
                    break;
                }
            }
            if (retType!=='' && 'ilfdz'.indexOf(retType) >= 0) {
                let result = new DataView(await this.client.conn.call(
                    0, this.value!, args2.buffer.slice(0, writeAt),
                    'ld'.indexOf(retType) >= 0 ? 8 : 4));
                let result2 = null;
                switch (retType) {
                    case 'i':
                        result2 = result.getInt32(0, true);
                        break;
                    case 'l':
                        result2 = result.getBigInt64(0, true);
                        break;
                    case 'f':
                        result2 = result.getFloat32(0, true);
                        break;
                    case 'd':
                        result2 = result.getFloat64(0, true);
                        break;
                    case 'z':
                        result2 = result.getInt32(0, true) != 0;
                        break;
                }
                return result2
            }else{
                let destAddr=await this.client.allocSlot()
                let status = new DataView(await this.client.conn.call(
                    destAddr, this.value!, args2.buffer.slice(0, writeAt), 4)).getUint32(0,true);
                let result=new RpcExtendClientObject(this.client,destAddr);
                if(retType=='s'){
                    freeBeforeReturn.push(destAddr);
                    if(status==1){
                        await this.client.checkException(result);
                    }else{
                        let byteData=await result.tryPull()
                        if(byteData!=null){
                            let t2=new TextDecoder().decode(byteData);
                            return t2
                        }else{
                            return null;
                        }
                    }
                }else if(retType=='b'){
                    freeBeforeReturn.push(destAddr);
                    if(status==1){
                        await this.client.checkException(result);
                    }else{
                        let byteData=await result.tryPull();
                        return byteData;
                    }
                }else{
                    if(status==1){
                        await this.client.checkException(result);
                    }else{
                        return result
                    }
                }
            }
        } finally {
            for (let t1 of freeBeforeReturn) {
                await this.client.freeSlot(t1);
            }
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

    protected builtIn?:{checkException?:RpcExtendClientCallable}
    public async ensureBuiltIn(){
        if(this.builtIn==undefined){
            this.builtIn={}
            let t1=await this.getFunc('builtin.checkException');
            if(t1!=null){
                t1.signature('o->s');
                this.builtIn.checkException=t1;
            }
        }
    }
    public async checkException(obj:RpcExtendClientObject){
        await this.ensureBuiltIn();
        if(this.builtIn!.checkException!=null){
            let err=await this.builtIn!.checkException.call(obj) as string
            if(err!=''){
                throw(new RpcExtendError(err));
            }
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
    protected paramBufLen=0;
    public constructor(public wrapped:(...args:any)=>Promise<any>){
    }
    //See RpcExtendClientCallable.signature
    public signature(sign: string) {
        let [tParam,tResult]=sign.split('->');
        this.tParam=tParam;
        this.tResult=tResult;
        this.paramBufLen=0;
        for(let i1=0;i1<tParam.length;i1++){
            if('ifobzs'.indexOf(tParam.charAt(i1))>=0){
                this.paramBufLen+=4;
            }else if('ld'.indexOf(tParam.charAt(i1))>=0){
                this.paramBufLen+=8;
            }
        }
        return this;
    }
    public async readParameter (req: PxpRequest){
        let buf=new DataView(await req.context.io1.read(this.paramBufLen));
        let tParam=this.tParam;
        let param=[];
        let offset=0;
        let obj:any=null;
        for(let i1=0;i1<tParam.length;i1++){
            switch(tParam[i1]){
                case 'i':
                    param.push(buf.getInt32(offset,true));
                    offset+=4;
                    break;
                case 'f':
                    param.push(buf.getFloat32(offset,true));
                    offset+=4;
                    break;
                case 'o':
                case 'b':
                    obj=req.context.refSlots[buf.getInt32(offset,true)]!.get();
                    param.push(obj);
                    offset+=4;
                    break;
                case 's':
                    obj=req.context.refSlots[buf.getInt32(offset,true)]!.get();
                    if(obj instanceof ArrayBuffer){
                        param.push(new TextDecoder().decode(obj));
                    }else{
                        param.push(obj);
                    }
                    offset+=4;
                    break;
                case 'l':
                    param.push(buf.getBigInt64(offset,true));
                    offset+=8;
                    break;
                case 'd':
                    param.push(buf.getFloat64(offset,true));
                    offset+=8;
                    break;
                default:
                    throw new Error('Unsupported value type.');
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
        let buf=new DataView(new ArrayBuffer(8));
        let len=0;
        switch(this.tResult){
            case 'i':
                buf.setInt32(0,req.result,true);len=4;
                break;
            case 'f':
                buf.setFloat32(0,req.result,true);len=4;
                break;
            case 'o':
            case 'b':
            case 's':
            case '':
                if(req.result instanceof Error){
                    buf.setInt32(0,1,true);
                }else{
                    buf.setInt32(0,0,true);
                }
                len=4;
                break;
            case 'l':
                buf.setBigInt64(0,req.result,true);len=8;
                break;
            case 'd':
                buf.setFloat64(0,req.result,true);len=8;
                break;
            default:
                throw new Error('Unsupported value type.');
        }
        await req.context.io1.write(buf.buffer.slice(0,len));
    }
}
var builtinServerFuncMap:{[k:string]:RpcExtendServerCallable}={
    'builtin.checkException':new RpcExtendServerCallable(async(e:any)=>(e instanceof Error)?e.message:'').signature('o->s')
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
        if((flag & this.FLAG_NO_HEADER_NAME)==0){
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
