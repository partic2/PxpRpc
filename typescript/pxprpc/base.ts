

export interface Io{
    receive():Promise<Uint8Array>;
    send(data:Uint8Array[]):Promise<void>;
    close():void;
}

export class Serializer{
    public dv?:DataView;
    public pos:number=0;
    public prepareUnserializing(buf:Uint8Array){
        this.dv=new DataView(buf.buffer,buf.byteOffset,buf.byteLength);
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
        if(val===0xff){
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
            let buf=new Uint8Array(newSize);
            buf.set(new Uint8Array(this.dv!.buffer,this.dv!.byteOffset,this.pos),0);
            this.dv=new DataView(buf.buffer,buf.byteOffset,buf.byteLength);
        }
    }
    public putBytes(b:Uint8Array){
        let len=b.byteLength;
        this.putVarint(len);
        this.ensureBuffer(len);
        new Uint8Array(this.dv!.buffer).set(b,this.dv!.byteOffset+this.pos);
        this.pos+=len;
        return this;
    }
    public putString(val:string){
        this.putBytes(new TextEncoder().encode(val));
        return this;
    }
    public build():Uint8Array{
        return new Uint8Array(this.dv!.buffer,this.dv!.byteOffset,this.pos)
    }
    public getBytes():Uint8Array{
        let len=this.getVarint();
        let val:Uint8Array=new Uint8Array(this.dv!.buffer,this.dv!.byteOffset+this.pos,len);
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
    protected waitingSessionCb={} as {[key:number]:(err:Error|null,result?:[number,Uint8Array])=>void}
    protected respReadingCb:(e:Error|null)=>void=()=>{};
    public async run(){
        if(this.running)return;
        this.running=true;
        try{
            while(this.running){
                let buf=await this.io1.receive();
                let packet=new DataView(buf.buffer,buf.byteOffset,buf.byteLength);
                let sid=packet.getUint32(0,true);
                let cb=this.waitingSessionCb[sid&0x7fffffff];
                cb(null,[sid,new Uint8Array(buf.buffer,buf.byteOffset+4,buf.byteLength-4)]);
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
    public async call(callableIndex:number,parameter:Uint8Array[],sid:number=0x100):Promise<Uint8Array>{
        let hdr=new Uint8Array(8);
        let hdr2=new DataView(hdr.buffer);
        hdr2.setUint32(0,sid,true);
        hdr2.setInt32(4,callableIndex,true)
        let respFut=new Promise<[number,Uint8Array]>((resolve,reject)=>{
            this.waitingSessionCb[sid]=(err,resp)=>{
                if(err===null){resolve(resp!);}else{reject(err)};
            }
        });
        await this.io1.send([hdr,...parameter])
        let [sid2,result]=await respFut;
        delete this.waitingSessionCb[sid&0x7fffffff];
        if(sid!=sid2){
            throw new PxprpcRemoteError(new TextDecoder().decode(result));
        }
        return result;
    }
    public async getFunc(funcName:string,sid:number=0x100){
        let result=await this.call(-1,[new TextEncoder().encode(funcName)],sid)
        return new DataView(result.buffer,result.byteOffset,result.byteLength).getInt32(0,true);
    }
    public async freeRef(index:number[],sid:number=0x100){
        let para=new DataView(new ArrayBuffer(index.length<<2));
        for(let i=0;i<index.length;i++){
            para.setInt32(i<<2,index[i],true);
        }
        await this.call(-2,[new Uint8Array(para.buffer,para.byteOffset,para.byteLength)],sid);
    }
    public async close(sid:number=0x100){
        let hdr=new Uint8Array(8);
        let hdr2=new DataView(hdr.buffer);
        hdr2.setUint32(0,sid,true);
        hdr2.setInt32(4,-3,true)
        await this.io1.send([hdr])
        this.running=false;
        this.io1.close();
    }
    public async getInfo(sid:number=0x100){
        let result=await this.call(-4,[],sid);
        return new TextDecoder().decode(result);
    }
    public poll(callableIndex:number,parameter:Uint8Array[],onResult:(err:Error|null,result?:Uint8Array)=>void,sid:number=0x100){
        let hdr=new Uint8Array(12);
        let hdr2=new DataView(hdr.buffer);
        hdr2.setUint32(0,sid,true);
        hdr2.setInt32(4,-5,true);
        hdr2.setInt32(8,callableIndex,true);
        let cb=(err: Error | null, result?: [number, Uint8Array])=>{
            if(err===null){
                if(sid!=result![0]){
                    delete this.waitingSessionCb[sid&0x7fffffff];
                    onResult(new PxprpcRemoteError(new TextDecoder().decode(result![1])))
                }else{
                    onResult(null,result![1]);
                }
            }else{onResult(err)};
        }
        this.waitingSessionCb[sid]=cb;
        this.io1.send([hdr,...parameter]).catch((err)=>onResult(err));
    }
}

export class PxpRequest{
    public callableIndex=-1;
    public parameter?:Uint8Array;
    public result:Uint8Array[]=[];
    public rejected:Error|null=null;
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
    public freeRefEntry:PxpRef|null=null;
    public constructor(public io1:Io){
    }
    public running=false;

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
        let dv=new DataView(r.parameter!.buffer,r.parameter!.byteOffset,r.parameter!.byteLength);
        for(let i=0;i<r.parameter!.byteLength;i+=4){
            this.freeRef(this.getRef(dv.getInt32(i,true)))
        }
    }
    public async closeHandler(r:PxpRequest){
        this.close();
    }
    protected builtInCallable=[null,this.getFunc,this.freeRefHandler,this.closeHandler,this.getInfo,this.poll]
    public async processRequest(r:PxpRequest) {
        try{
            if(r.callableIndex>=0){
                await (this.getRef(r.callableIndex).object as PxpCallable).call(r);
            }else{
                await this.builtInCallable[-r.callableIndex]!.call(this,r);
            }
            //-3 close, don't response.Also -5 poll will set the callableIndex to -3 to avoid response.
            if(r.callableIndex===-3)return;
            let sid=new DataView(new ArrayBuffer(4));
            if(r.rejected===null){
                sid.setUint32(0,r.session,true);
                await this.io1.send([new Uint8Array(sid.buffer),...r.result])
            }else{
                sid.setUint32(0,r.session^0x80000000,true);
                await this.io1.send([new Uint8Array(sid.buffer),new TextEncoder().encode(String(r.rejected))])
            }
        }catch(e){
            this.close();
            // mute all error here.
        }
    }
    public async serve(){
        if(this.running)return;
        this.running=true;
        try{
            while(this.running) {
                let buf=await this.io1.receive();
                let packet=new DataView(buf.buffer,buf.byteOffset,buf.byteLength);
                let r=new PxpRequest(this,packet.getUint32(0,true));
                r.callableIndex=packet.getInt32(4,true);
                r.parameter=new Uint8Array(buf.buffer,buf.byteOffset+8,buf.byteLength-8);
                this.processRequest(r);
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
        r.result=[new Uint8Array(res.buffer)];
	}
	public async getInfo(r:PxpRequest){
        r.result=[new TextEncoder().encode(
            "server name:pxprpc for typescript\n"+
        "version:2.0\n")]
	}
    public async poll(r:PxpRequest){
        let view=new DataView(r.parameter!.buffer,r.parameter!.byteOffset,r.parameter!.byteLength);
        r.callableIndex=view.getInt32(0,true);
        r.parameter=new Uint8Array(r.parameter!.buffer,r.parameter!.byteOffset+4,r.parameter!.byteLength-4);
        while(this.running){
            r.rejected=null;
            r.result=[];
            await this.processRequest(r);
            if(r.rejected!==null)break;
        }
        r.callableIndex=-3;
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