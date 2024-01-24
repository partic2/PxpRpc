import { Client, PxpCallable, PxpRequest, Serializer, Server } from './base'


export class RpcExtendError extends Error {
}

export class RpcExtendClientObject {
    public constructor(public client: RpcExtendClient1, public value: number | undefined) {
    }
    public async free() {
        if (this.value != undefined) {
            await this.client.freeSid(this.value);
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
function typedecl
format: 'parameters type->return type' 
eg:
a function defined in c:
bool fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
defined in java:
boolean fn(int,int,double,Object)
...
it's pxprpc typedecl: 
iido->c

available type typedecl characters:
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
    public typedecl(decl: string) {
        [this.tParam,this.tResult]=decl.split('->');
        return this;
    }

    public async call(...args:any[]) {
        let buf:ArrayBuffer[]=[]
        if(this.tParam==='b'){
            let abuf=args[0] as ArrayBuffer
            buf=[abuf];
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
                        if(args[i1]!==null){
                            ser.putInt(args[i1].value);
                        }else{
                            ser.putInt(-1);
                        }
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
            buf=[serbuf];
        }
        let sid=this.client.allocSid();
        let resp=await this.client.conn.call(this.value!,buf,sid);
        this.client.freeSid(sid);
        if(this.tResult==='b'){
            return resp;
        }else{
            let ser=new Serializer().prepareUnserializing(resp);
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
                        let val=ser.getInt();
                        if(val!==-1){
                            rets.push(new RpcExtendClientObject(this.client,val))
                        }else{
                            rets.push(null)
                        }
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
    }
}



export class RpcExtendClient1 {
    private __usedSid: { [index: number]: boolean | undefined } = {};
    private __nextSid: number;

    private __sidStart: number = 1;
    private __sidEnd: number = 0xffff;
    
    public constructor(public conn: Client) {
        this.__nextSid = this.__sidStart;
    }


    public async init():Promise<this>{
        if(!this.conn.isRunning()){
            this.conn.run();
        }
        return this;
    }

    protected builtIn?:{}
    public async ensureBuiltIn(){
        if(this.builtIn==undefined){
            this.builtIn={}
        }
    }
    public allocSid() {
        let reachEnd = false;
        while (this.__usedSid[this.__nextSid]) {
            this.__nextSid += 1
            if (this.__nextSid >= this.__sidEnd) {
                if (reachEnd) {
                    throw new RpcExtendError('No sid available')
                } else {
                    reachEnd = true
                    this.__nextSid = this.__sidStart
                }
            }
        }

        let t1 = this.__nextSid
        this.__nextSid += 1
        if(this.__nextSid>=this.__sidEnd){
            this.__nextSid=this.__sidStart;
        }
        this.__usedSid[t1] = true;
        return t1
    }
    public freeSid(index: number) {
        delete this.__usedSid[index]
    }
    public async getFunc(name: string) {
        let index=await this.conn.getFunc(name,this.__sidEnd+1);
        return new RpcExtendClientCallable(this, index)
    }
    public async close(){
        await this.conn.close()
    }

}

export function allocRefFor(serv:Server,obj:any){
    let ref=serv.allocRef();
    ref.object=obj;
    if((typeof obj==='object') && ('close' in obj)){
        ref.onFree=()=>obj.close()
    }
    return ref;
}

export class RpcExtendServerCallable implements PxpCallable{
    protected tParam:string = '';
    protected tResult:string='';
    public constructor(public wrapped:(...args:any)=>Promise<any>){
    }
    //See RpcExtendClientCallable.typedecl
    public typedecl(decl: string) {
        let [tParam,tResult]=decl.split('->');
        this.tParam=tParam;
        this.tResult=tResult;
        return this;
    }
    public readParameter (req: PxpRequest){
        let buf=req.parameter!
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
                        let val=ser.getInt();
                        if(val!==-1){
                            obj=req.context.getRef(val).object;
                            param.push(obj);
                        }else{
                            param.push(null)
                        }
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
        return param;
    }
    public async call(req: PxpRequest) : Promise<any>{
        try{
            let r=await this.wrapped.apply(this,this.readParameter(req));
            req.result=await this.writeResult(req,r);
        }catch(e){
            return req.rejected=e as Error;
        }
    }
    public writeResult(req: PxpRequest,r:any):ArrayBuffer[]{
        let ser=new Serializer().prepareSerializing(8);
        if(this.tResult==='b'){
            return [r]
        }else{
            let results=this.tResult.length>1?r:[r]
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
                        if(results[i]!==null){
                            ser.putInt(allocRefFor(req.context,results[i]).index);
                        }else{
                            ser.putInt(-1);
                        }
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
            if(buf.byteLength==0)return [];
            return [buf];
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
    public setHeader(types:string|null,names:string[]|null){
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
        let rowCnt=ser.getVarint();
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
                case 'c':
                    for(let i2=0;i2<rowCnt;i2++){
                        this.rows[i2][i1]=ser.getVarint()!==0;
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
        if(this.headerType==null){
            this.headerType=''
            if(this.rows.length>=1){
                for(let t1 of this.rows[0]){
                    switch(typeof t1){
                        case 'number':
                            this.headerType+='d';
                            break;
                        case 'string':
                            this.headerType+='s';
                            break;
                        case 'boolean':
                            this.headerType+='c';
                            break;
                        case 'bigint':
                            this.headerType+='l';
                            break;
                        default:
                            if(t1 instanceof ArrayBuffer){
                                this.headerType+='b'
                            }else{
                                this.headerType+='o'
                            }
                            break;
                    }
                }
            }
        }
        let colsCnt=this.headerType!.length;
        let flag=0;
        if(this.headerName==null){
            flag|=this.FLAG_NO_HEADER_NAME;
        }
        ser.putInt(flag);
        let rowCnt=this.rows.length;
        ser.putVarint(rowCnt);
        ser.putString(this.headerType!);
        if(this.headerName!==null){
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
                case 'c':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putVarint(this.rows[i2][i1]?1:0);
                    }
                    break;
                default:
                    throw new Error("Unknown Type");
            }
        }
        return ser.build();
    }
    public toMapArray():any[]{
        let r:any[]=[]
        let rowCount=this.getRowCount()
        let colCount=this.headerName!.length;
        for(let t1=0;t1<rowCount;t1++){
            let r0={} as any;
            let row=this.getRow(t1);
            for(let t2=0;t2<colCount;t2++){
                r0[this.headerName![t2]]=row[t2];
            }
            r.push(r0);
        }
        return r
    }
    public fromMapArray(val:any[]):this{
        if(val.length>0 && this.headerName===null){
            this.headerName=[];
            for(let k in val[0]){
                this.headerName.push(k);
            }
        }
        let rowCount=val.length;
        let colCount=this.headerName!.length;
        for(let t1=0;t1<rowCount;t1++){
            let row=[];
            for(let t2=0;t2<colCount;t2++){
                row.push(val[t1][this.headerName![t2]]);
            }
            this.addRow(row);
        }
        return this
    }
}
