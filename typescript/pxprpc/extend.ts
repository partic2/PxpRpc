import { Client, PxpCallable, PxpRequest, Serializer, Server } from './base'


export class RpcExtendError extends Error {
}

export class RpcExtendClientObject {
    public constructor(public client: RpcExtendClient1, public value: number | undefined) {
    }
    public async free() {
        if (this.value != undefined) {
            let sid=this.client.allocSid();
            try{
                await this.client.conn.freeRef([this.value],sid)
            }finally{
                await this.client.freeSid(this.value);
            }
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
            new TableSerializer().
                bindContext(null,this.client).bindSerializer(ser).setHeader(this.tParam,null).
                putRowsData([args])
            let serbuf=ser.build()
            buf=[serbuf];
        }
        let sid=this.client.allocSid();
        try{
            let resp=await this.client.conn.call(this.value!,buf,sid);
            if(this.tResult==='b'){
                return resp;
            }else{
                let ser=new Serializer().prepareUnserializing(resp);
                let rets=new TableSerializer().
                    bindContext(null,this.client).bindSerializer(ser).setHeader(this.tResult,null).
                    getRowsData(1)[0]
                if(this.tResult.length===1){
                    return rets[0];
                }else if(this.tResult.length===0){
                    return null;
                }else{
                    return rets;
                }
            }
        }finally{
            this.client.freeSid(sid);
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
        let sid=this.allocSid();
        try{
            let index=await this.conn.getFunc(name,sid);
            return new RpcExtendClientCallable(this, index)
        }finally{
            this.freeSid(sid);
        }
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
            param=new TableSerializer().
                bindContext(req.context,null).bindSerializer(ser).setHeader(this.tParam,null).
                getRowsData(1)[0]
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
        if(this.tResult==='b'){
            return [r]
        }else{
            let ser=new Serializer().prepareSerializing(8);
            let results=this.tResult.length>1?r:[r]
            new TableSerializer().
                bindContext(req.context,null).bindSerializer(ser).setHeader(this.tResult,null).
                putRowsData([results]);
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
    rows:any[][]=[];
    boundServContext:Server|null=null;
    boundClieContext:RpcExtendClient1|null=null;
    ser:Serializer|null=null;
    public setHeader(types:string|null,names:string[]|null){
        this.headerName=names;
        this.headerType=types;
        return this;
    }
    public bindContext(serv:Server|null,clie:RpcExtendClient1|null){
        this.boundServContext=serv;
        this.boundClieContext=clie;
        return this;
    }
    public bindSerializer(ser:Serializer){
        this.ser=ser;
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
    public getRowsData(rowCnt:number):any[][]{
        let rows:any[][]=[];
        let colCnt=this.headerType!.length;
        let ser=this.ser!;
        for(let i1=0;i1<rowCnt;i1++){
            rows.push(new Array(colCnt));
        }
        for(let i1=0;i1<colCnt;i1++){
            let type=this.headerType!.charAt(i1);
            switch(type){
                case 'i':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getInt();
                    }
                    break;
                case 'l':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getLong();
                    }
                    break;
                case 'f':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getFloat();
                    }
                    break;
                case 'd':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getDouble();
                    }
                    break;
                case 'b':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getBytes();
                    }
                    break;
                case 's':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getString();
                    }
                    break;
                case 'c':
                    for(let i2=0;i2<rowCnt;i2++){
                        rows[i2][i1]=ser.getVarint()!==0;
                    }
                    break;
                case 'o':
                    for(let i2=0;i2<rowCnt;i2++){
                        let val=ser.getInt();
                        if(val===-1){
                            rows[i2][i1]=null;
                        }else{
                            if(this.boundServContext!=null){
                                rows[i2][i1]=this.boundServContext.getRef(val).object;
                            }else{
                                rows[i2][i1]=new RpcExtendClientObject(this.boundClieContext!,val);
                            }
                        }
                    }
                    break;
                default:
                    throw new Error("Unknown Type");
            }
        }
        return rows;
    }
    public load(buf:ArrayBuffer){
        if(buf!=null){
            this.bindSerializer(new Serializer().prepareUnserializing(buf))
        }
        let ser=this.ser!;
        let flag=ser.getVarint();
        let rowCnt=ser.getVarint();
        this.headerType=ser.getString();
        let colCnt=this.headerType.length;
        if((flag & this.FLAG_NO_HEADER_NAME)===0){
            this.headerName=[]
            for(let i1=0;i1<colCnt;i1++){
                this.headerName.push(ser.getString());
            }
        }
        this.rows=this.getRowsData(rowCnt);
        return this;
    }
    public putRowsData(rows:any[][]){
        let colCnt=this.headerType!.length;
        let rowCnt=rows.length;
        let ser=this.ser!;
        for(let i1=0;i1<colCnt;i1++){
            let type=this.headerType!.charAt(i1);
            switch(type){
                case 'i':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putInt(rows[i2][i1]);
                    }
                    break;
                case 'l':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putLong(rows[i2][i1]);
                    }
                    break;
                case 'f':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putFloat(rows[i2][i1]);
                    }
                    break;
                case 'd':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putDouble(rows[i2][i1]);
                    }
                    break;
                case 'b':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putBytes(rows[i2][i1]);
                    }
                    break;
                case 's':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putString(rows[i2][i1]);
                    }
                    break;
                case 'c':
                    for(let i2=0;i2<rowCnt;i2++){
                        ser.putVarint(rows[i2][i1]?1:0);
                    }
                    break;
                case 'o':
                    for(let i2=0;i2<rowCnt;i2++){
                        let val=rows[i2][i1];
                        if(val!==null){
                            if(this.boundServContext!=null){
                                ser.putInt(allocRefFor(this.boundServContext,val).index);
                            }else{
                                ser.putInt(val.value);
                            }
                        }else{
                            ser.putInt(-1);
                        }
                    }
                    break;
                default:
                    throw new Error("Unknown Type");
            }
        }
    }
    public build():ArrayBuffer{
        if(this.ser==null){
            this.bindSerializer(new Serializer().prepareSerializing(64));
        }
        let ser=this.ser!;
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
        let flag=0;
        if(this.headerName==null){
            flag|=this.FLAG_NO_HEADER_NAME;
        }
        ser.putVarint(flag);
        let rowCnt=this.rows.length;
        ser.putVarint(rowCnt);
        ser.putString(this.headerType!);
        if(this.headerName!==null){
            for(let e of this.headerName){
                ser.putString(e);
            }
        }
        this.putRowsData(this.rows);
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
    public toArray():any[]{
        let r:any[]=[]
        let rowCount=this.getRowCount()
        for(let t1=0;t1<rowCount;t1++){
            r.push(this.getRow(t1)[0]);
        }
        return r 
    }
    public fromArray(val:any[]){
        let rowCount=val.length;
        for(let t1=0;t1<rowCount;t1++){
            this.addRow([val[t1]]);
        }
        return this
    }
}
