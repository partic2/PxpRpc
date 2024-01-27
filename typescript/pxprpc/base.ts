

export interface Io{
    receive():Promise<ArrayBuffer>;
    send(data:ArrayBufferLike[]):Promise<void>;
    close():void;
}

export class Serializer{
    public dv?:DataView;
    public pos:number=0;
    public prepareUnserializing(buf:ArrayBuffer){
        this.dv=new DataView(buf);
        return this;
    }
    public prepareSerializing(initBufSize:number){
        this.dv=new DataView(new ArrayBuffer(initBufSize));
        return this;
    } 
    public getInt(){
        let val=this.dv!.getInt32(this.pos,true);
        this.pos+=4;
        return val;
    }
    public getLong(){
        let val=this.dv!.getBigInt64(this.pos,true);
        this.pos+=8;
        return val;
    }
    public getFloat(){
        let val=this.dv!.getFloat32(this.pos,true);
        this.pos+=4;
        return val;
    }
    public getDouble(){
        let val=this.dv!.getFloat64(this.pos,true);
        this.pos+=8;
        return val;
    }
    public getVarint(){
        let val=this.dv!.getUint8(this.pos);
        this.pos++;
        if(val==0xff){
            val=this.dv!.getUint32(this.pos,true);
            this.pos+=4;
        }
        return val;
    }
    public putInt(val:number){
        this.ensureBuffer(4);
        this.dv!.setInt32(this.pos,val,true);
        this.pos+=4;
        return this;
    }
    public putLong(val:bigint){
        this.ensureBuffer(8);
        this.dv!.setBigInt64(this.pos,val,true);
        this.pos+=8;
        return this;
    }
    public putFloat(val:number){
        this.ensureBuffer(4);
        this.dv!.setFloat32(this.pos,val,true);
        this.pos+=4;
        return this;
    }
    public putDouble(val:number){
        this.ensureBuffer(8);
        this.dv!.setFloat64(this.pos,val,true);
        this.pos+=8;
        return this;
    }
    public putVarint(val:number){
        if(val>=0xff){
            this.ensureBuffer(5);
            this.dv!.setUint8(this.pos,0xff);
            this.pos+=1;
            this.dv!.setUint32(this.pos,val,true);
            this.pos+=4;
        }else{
            this.ensureBuffer(1);
            this.dv!.setUint8(this.pos,val);
            this.pos++;
        }
    }
    public ensureBuffer(remainSize:number){
        if(this.pos+remainSize>this.dv!.buffer.byteLength){
            let newSize=this.pos+remainSize;
            newSize+=newSize>>1;
            let buf=new ArrayBuffer(newSize);
            new Uint8Array(buf).set(new Uint8Array(this.dv!.buffer),0);
            this.dv=new DataView(buf);
        }
    }
    public putBytes(b:ArrayBuffer){
        let len=b.byteLength;
        this.putVarint(len);
        this.ensureBuffer(len);
        new Uint8Array(this.dv!.buffer).set(new Uint8Array(b),this.pos);
        this.pos+=len;
        return this;
    }
    public putString(val:string){
        this.putBytes(new TextEncoder().encode(val));
        return this;
    }
    public build(){
        return this.dv!.buffer.slice(0,this.pos);
    }
    public getBytes(){
        let len=this.getVarint();
        let val=this.dv!.buffer.slice(this.pos,this.pos+len);
        this.pos+=len;
        return val;
    }
    public getString(){
        return new TextDecoder().decode(this.getBytes());
    }
}

export class PxprpcRemoteError extends Error{}

export class Client{
    public constructor(public io1:Io){
    }
    protected running=false;
    protected waitingSessionCb={} as {[key:number]:(err:Error|null,result?:[number,ArrayBuffer])=>void}
    protected respReadingCb:(e:Error|null)=>void=()=>{};
    public async run(){
        this.running=true;
        try{
            while(this.running){
                let packet=new DataView(await this.io1.receive());
                let sid=packet.getUint32(0,true);
                let cb=this.waitingSessionCb[sid&0x7fffffff];
                delete this.waitingSessionCb[sid&0x7fffffff];
                cb(null,[sid,packet.buffer.slice(4)]);
            }
        }catch(e){
            for(let k in this.waitingSessionCb){
                let cb=this.waitingSessionCb[k];
                cb(e as any);
            }
        }finally{
            this.running=false;
        }
    }
    public isRunning():boolean{
        return this.running;
    }
    public async call(callableIndex:number,parameter:ArrayBuffer[],sid:number=0x100):Promise<ArrayBuffer>{
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        hdr2.setUint32(0,sid,true);
        hdr2.setInt32(4,callableIndex,true)
        let respFut=new Promise<[number,ArrayBuffer]>((resolve,reject)=>{
            this.waitingSessionCb[sid]=(err,resp)=>{
                if(err==null){resolve(resp!);}else{reject(err)};
            }
        });
        await this.io1.send([hdr,...parameter])
        let [sid2,result]=await respFut;
        if(sid!=sid2){
            throw new PxprpcRemoteError(new TextDecoder().decode(result));
        }
        return result;
    }
    public async getFunc(funcName:string,sid:number=0x100){
        let result=await this.call(-1,[new TextEncoder().encode(funcName)],sid)
        return new DataView(result).getInt32(0,true);
    }
    public async freeRef(index:number[],sid:number=0x100){
        let para=new DataView(new ArrayBuffer(index.length<<2));
        for(let i=0;i<index.length;i++){
            para.setInt32(i<<2,index[i],true);
        }
        await this.call(-2,[para.buffer],sid);
    }
    public async close(sid:number=0x100){
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        hdr2.setUint32(0,sid,true);
        hdr2.setInt32(4,-3,true)
        let respFut=new Promise<[number,ArrayBuffer]>((resolve,reject)=>{
            this.waitingSessionCb[sid]=(err,resp)=>{
                if(err==null){resolve(resp!);}else{reject(err)};
            }
        });
        await this.io1.send([hdr])
        this.running=false;
        this.io1.close();
    }
    public async getInfo(sid:number=0x100){
        let result=await this.call(-4,[],sid);
        return new TextDecoder().decode(result);
    }
    public async sequence(mask:number,maskCnt:number=24,sid:number=0x100){
        let hdr=new ArrayBuffer(4);
        new DataView(hdr).setUint32(0,mask|maskCnt,true);
        return await this.call(-5,[hdr],sid);
    }
}

export class PxpRequest{
    public callableIndex=-1;
    public parameter?:ArrayBuffer;
    public result:ArrayBuffer[]=[];
    public rejected:Error|null=null;
    public nextPending:PxpRequest|null=null;
    public inSequence=false;
    public constructor(public context:Server,public session:number){}
}

export class PxpRef{
    public object:any=null;
    public onFree:null|(()=>void)=null;
    public nextFree:PxpRef|null=null;
    public constructor(public index:number){}
}

export interface PxpCallable{
    call:(req:PxpRequest)=>Promise<any>,
}

let RefPoolExpandCount=256;

export class Server{
    public refPool=new Array<PxpRef>();
    public sequenceSession=0xffffffff;
    public sequenceMaskBitsCnt=0;
    public freeRefEntry:PxpRef|null=null;
    public constructor(public io1:Io){
    }
    public running=false;
    
    pendingRequests:{[sid:number]:PxpRequest}={};
    public queueRequest(r:PxpRequest){
        if(this.sequenceSession==0xffffffff || (r.session>>>(32-this.sequenceMaskBitsCnt)!=this.sequenceSession)){
            this.processRequest(r);
            return;
        }
        r.inSequence=true;
        let r2=this.pendingRequests[r.session];
        if(r2==undefined){
            this.pendingRequests[r.session]=r;
            this.processRequest(r);
        }else{
            while(r2.nextPending!=null){
                r2=r2.nextPending;
            }
            r2.nextPending=r;
        }
    }
    //return next request to process
    public finishRequest(r:PxpRequest){
        if(r.inSequence){
            if(r.nextPending!=null){
                this.pendingRequests[r.session]=r.nextPending;
                return r.nextPending;
            }else{
                delete this.pendingRequests[r.session];
                return null;
            }
        }else{
            return null;
        }
    }
    public expandRefPools(){
        let start=this.refPool.length;
        let end=this.refPool.length+RefPoolExpandCount-1;
        for(let i=start;i<=end;i++){
            this.refPool.push(new PxpRef(i));
        }
        for(let i=start;i<end;i++){
            this.refPool[i].nextFree=this.refPool[i+1];
        }
        this.refPool[end].nextFree=this.freeRefEntry;
        this.freeRefEntry =this.refPool[start];
    }
    public allocRef(){
        if(this.freeRefEntry==null){
            this.expandRefPools();
        }
        let ref2 = this.freeRefEntry;
        this.freeRefEntry = this.freeRefEntry!.nextFree;
        return ref2!;
    }
    public freeRef(ref2:PxpRef){
        if(ref2.onFree!=null){
            ref2.onFree();
            ref2.onFree=null;
        }
        ref2.object=null;
        ref2.nextFree=this.freeRefEntry;
        this.freeRefEntry=ref2;
    }
    public getRef(index:number){
        return this.refPool[index];
    }
    public async freeRefHandler(r:PxpRequest){
        let dv=new DataView(r.parameter!);
        for(let i=0;i<r.parameter!.byteLength;i+=4){
            this.freeRef(this.getRef(dv.getInt32(i,true)))
        }
    }
    public async closeHandler(r:PxpRequest){
        this.close();
    }
    protected builtInCallable=[null,this.getFunc,this.freeRefHandler,this.closeHandler,this.getInfo,this.sequence]
    public async processRequest(r:PxpRequest|null) {
        while(r!=null){
            if(r.callableIndex>=0){
                await (this.getRef(r.callableIndex).object as PxpCallable).call(r);
            }else{
                await this.builtInCallable[-r.callableIndex]!.call(this,r);
            }
            let sid=new DataView(new ArrayBuffer(4));
            if(r.rejected==null){
                sid.setUint32(0,r.session,true);
                await this.io1.send([sid.buffer,...r.result])
            }else{
                sid.setUint32(0,r.session^0x80000000,true);
                await this.io1.send([sid.buffer,new TextEncoder().encode(String(r.rejected))])
            }
            r=this.finishRequest(r);
        }
    }
    public async serve(){
        this.running=true;
        try{
            while(this.running) {
                let packet=new DataView(await this.io1.receive());
                let r=new PxpRequest(this,packet.getUint32(0,true));
                r.callableIndex=packet.getInt32(4,true);
                r.parameter=packet.buffer.slice(8);
                this.queueRequest(r);
            }
        }finally{
            this.close()
        }
		
	}
    public funcMap:((name:string)=>PxpCallable|null)|null=null;
	public async getFunc(r:PxpRequest){
		let name=new TextDecoder().decode(r.parameter!);
		let found=this.funcMap?.(name);
        let res=new DataView(new ArrayBuffer(4));
        if(found==null){
           res.setInt32(0,-1,true);
        }else{
            let ref2=this.allocRef();
            ref2.object=found;
            res.setInt32(0,ref2.index,true);
        }
        r.result=[res.buffer];
	}
	public async getInfo(r:PxpRequest){
        r.result=[new TextEncoder().encode(
            "server name:pxprpc for typescript\n"+
        "version:2.0\n")]
	}
    public async sequence(r:PxpRequest){
        this.sequenceSession=new DataView(r.parameter!).getUint32(0,true);
        if(this.sequenceSession==0xffffffff){
            //discard pending request. execute immdiately mode, default value
            for(let i2 in this.pendingRequests){
                let r2=this.pendingRequests[i2];
                r2.nextPending=null;
            }
            this.pendingRequests=this.pendingRequests;
        }else{
            this.sequenceMaskBitsCnt=this.sequenceSession&0xff;
            this.sequenceSession=this.sequenceSession>>>(32-this.sequenceMaskBitsCnt);
        }
    }
    public close(){
        if(!this.running)return;
        this.running=false;
        this.io1.close();
        for(let ref2 of this.refPool){
            if(ref2.onFree!=null){
                ref2.onFree();
                ref2.onFree=null;
            }
            ref2.object=null;
        }
    }
}