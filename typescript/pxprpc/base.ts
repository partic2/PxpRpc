

export interface Io{
    read(size:number):Promise<ArrayBuffer>;
    write(data:ArrayBufferLike):Promise<void>;
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

class mutex{
    protected locked:boolean=false;
    protected unlockCb:Array<()=>void>=[];
    constructor(){
    }
    public async lock(){
        var that=this;
        if(this.locked){
            return new Promise<void>(function(resolve,reject){
                that.unlockCb.push(resolve);
            });
        }else{
            this.locked=true;
            return;
        }
    }
    public async unlock(){
        if(this.unlockCb.length>0){
            this.unlockCb.shift()!();
        }else{
            this.locked=false;
        }
    }
    public async tryLock(){
        if(!this.locked){
            this.locked=true;
            return true;
        }else{
            return false;
        }
    }
    public async mutexDo(fn:()=>Promise<any>){
        await this.lock();
        try{await fn();}finally{await this.unlock()}
    }
}

export class Client{
    protected nextSid1=0;
    protected writeLock=new mutex();
    public constructor(public io1:Io){
        this.nextSid1=0;
    }
    protected running=false;
    protected waitingSessionCb={} as {[key:number]:(e:Error|null)=>void}
    protected respReadingCb:(e:Error|null)=>void=()=>{};
    public async run(){
        this.running=true;
        try{
            while(this.running){
                let sid=await this.readUint32();
                let cb=this.waitingSessionCb[sid];
                delete this.waitingSessionCb[sid];
                let respReadingDone=new Promise<undefined>((resolve,reject)=>{
                    this.respReadingCb=(e)=>{
                        if(e==null){resolve(undefined)}else{reject(e)};
                    }
                });
                cb(null);
                await respReadingDone;
            }
        }catch(e){
            for(let k in this.waitingSessionCb){
                let cb=this.waitingSessionCb[k];
                cb(e as any);
            }
        }
    }
    public isRunning():boolean{
        return this.running;
    }
    public async readUint32(){
        let buf=await this.io1.read(4);
        return new DataView(buf).getUint32(0,true);
    }
    public async readInt32(){
        let buf=await this.io1.read(4);
        return new DataView(buf).getInt32(0,true); 
    }
    public async push(destAddr:number,data:ArrayBufferLike,sid:number=0x100){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        sid=sid|1;
        hdr2.setUint32(0,sid,true);
        hdr2.setUint32(4,destAddr,true);
        hdr2.setUint32(8,data.byteLength,true);
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
            await this.io1.write(data);
        })
        await respFut;
        this.respReadingCb(null);
    }
    public async pull(srcAddr:number,sid:number=0x100){
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        sid=sid|2;
        hdr2.setUint32(0,sid,true);
        hdr2.setUint32(4,srcAddr,true)
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        await this.writeLock.unlock();
        await respFut;
        let size=await this.readInt32();
        let result:ArrayBufferLike|null
        if(size==-1){
            result=null;
        }else{
            result=await this.io1.read(size);
        }
        this.respReadingCb(null);
        return result;
    }
    public async assign(destAddr:number,srcAddr:number,sid:number=0x100){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        sid=sid|3;
        hdr2.setUint32(0,sid,true);
        hdr2.setUint32(4,destAddr,true);
        hdr2.setUint32(8,srcAddr,true);
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        await respFut;
        this.respReadingCb(null);
    }
    public async unlink(destAddr:number,sid:number=0x100){
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        sid=sid|4;
        hdr2.setUint32(0,await sid,true);
        hdr2.setUint32(4,destAddr,true)
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        await respFut;
        this.respReadingCb(null);
    }
    public async call(destAddr:number,fnAddr:number,args:ArrayBufferLike[],onResponse:(io1:Io)=>Promise<void>,sid:number=0x100){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        sid=sid|5;
        hdr2.setUint32(0,await sid,true);
        hdr2.setUint32(4,destAddr,true)
        hdr2.setUint32(8,fnAddr,true)
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
            for(let buf of args){
                await this.io1.write(buf);
            }
        });
        await respFut;
        await onResponse(this.io1);
        this.respReadingCb(null);
    }
    public async getFunc(destAddr:number,fnNameAddr:number,sid:number=0x100){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        sid=sid|6;
        hdr2.setUint32(0,sid,true);
        hdr2.setUint32(4,destAddr,true);
        hdr2.setUint32(8,fnNameAddr,true);
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        await respFut;
        let data1=await this.readUint32()
        this.respReadingCb(null);
        return data1
    }
    public async close(sid:number=0x100){
        let hdr=new ArrayBuffer(4);
        let hdr2=new DataView(hdr);
        sid=sid|7
        hdr2.setUint32(0,sid,true);
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        this.running=false;
    }
    public async getInfo(sid:number=0x100){
        let hdr=new ArrayBuffer(4);
        let hdr2=new DataView(hdr);
        sid=sid|8;
        hdr2.setUint32(0,sid,true);
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        await respFut;
        let size=await this.readUint32();
        let data1=new TextDecoder().decode(await this.io1.read(size));
        this.respReadingCb(null);
        return data1;
    }
    public async sequence(mask:number,maskCnt:number=24,sid:number=0x100){
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        sid=sid|9;
        hdr2.setUint32(0,sid,true);
        hdr2.setUint32(4,mask|maskCnt,true);
        let respFut=new Promise((resolve,reject)=>{
            this.waitingSessionCb[sid]=(e)=>{
                if(e==null){resolve(e);}else{reject(e)};
            }
        });
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        await respFut;
        this.respReadingCb(null);
    }
    public async buffer(sid:number=0x100){
        let hdr=new ArrayBuffer(4);
        let hdr2=new DataView(hdr);
        sid=sid|10;
        hdr2.setUint32(0,sid,true);
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
    }
}

export class PxpRequest{
    public destAddr=0;
    public srcAddr=0;
    public funcAddr=0;
    public parameter:any;
    public result:any;
    public callable:PxpCallable|null=null;
    public opcode:number=0;
    public nextPending:PxpRequest|null=null;
    public inSequence=false;
    public constructor(public context:Server,public session:number){
        this.opcode=session&0xff;
    }
}

export interface PxpCallable{
    readParameter:(req:PxpRequest)=>Promise<void>,
    call:(req:PxpRequest)=>Promise<any>,
    writeResult:(req:PxpRequest)=>Promise<void>
}


export class PxpObject{
	protected count:number=0;
	protected content:any;
	public constructor(c:any) {
		this.count=0;
		this.content=c;
	}
	public addRef() {
		return ++this.count;
	}
	public release() {
		this.count--;
		if(this.count==0) {
			this.close();
		}
		return this.count;
	}
	public get() {
		return this.content;
	}
	public close(){
		if(this.content!==null && typeof this.content=='object' && 'close' in this.content) {
			this.content.close();
		}
	}
}


export class Server{
    public refSlots=new Array<PxpObject|null>();
    public sequenceSession=0xffffffff;
    public sequenceMaskBitsCnt=0;
    public constructor(public io1:Io){
    }
    public running=false;
    public async readUint32(){
        let buf=await this.io1.read(4);
        return new DataView(buf).getUint32(0,true);
    }
    public async readInt32(){
        let buf=await this.io1.read(4);
        return new DataView(buf).getInt32(0,true); 
    }
    public async writeUint32(u32:number){
        let buf=new ArrayBuffer(4);
        new DataView(buf).setUint32(0,u32,true);
        await this.io1.write(buf);
    }
    public async writeInt32(i32:number){
        let buf=new ArrayBuffer(4);
        new DataView(buf).setInt32(0,i32,true);
        await this.io1.write(buf);
    }
    pendingRequests:{[sid:number]:PxpRequest}={};
    public queueRequest(r:PxpRequest){
        if(this.sequenceSession==0xffffffff || (r.session>>(32-this.sequenceMaskBitsCnt)!=this.sequenceSession)){
            this.processRequest(r);
            return;
        }
        r.inSequence=true;
        let r2=this.pendingRequests[r.session>>8];
        if(r2==undefined){
            this.pendingRequests[r.session>>8]=r;
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
                this.pendingRequests[r.session>>8]=r.nextPending;
                return r.nextPending;
            }else{
                delete this.pendingRequests[r.session>>8];
                return null;
            }
        }else{
            return null;
        }
    }
    public async processRequest(r:PxpRequest|null) {
        while(r!=null){
            switch(r.opcode){
                case 1:
                    await this.push(r);
                    break;
                case 2:
                    await this.pull(r);
                    break;
                case 3:
                    await this.assign(r);
                    break;
                case 4:
                    await this.unlink(r);
                    break;
                case 5:
                    await this.call(r);
                    break;
                case 6:
                    await this.getFunc(r);
                    break;
                case 7:
                    this.close(r);
                    break;
                case 8:
                    await this.getInfo(r);
                    break;
                case 9:
                    await this.sequence(r);
                    break;
                case 10:
                    await this.buffer(r);
                    break;
            }
            r=this.finishRequest(r);
        }
    }
    public async serve(){
        this.running=true;
		while(this.running) {
			let session=await this.readUint32();
            let r=new PxpRequest(this,session);
			switch(r.opcode) {
			case 1:
				r.destAddr=await this.readInt32();
				let len=await this.readInt32();
				r.parameter=await this.io1.read(len);
				this.queueRequest(r);
				break;
			case 2:
				r.srcAddr=await this.readInt32();
				this.queueRequest(r);
				break;
			case 3:
				r.destAddr=await this.readInt32();
				r.srcAddr=await this.readInt32();
				this.queueRequest(r);
				break;
			case 4:
				r.destAddr=await this.readInt32();
				this.queueRequest(r);
				break;
			case 5:
				r.destAddr=await this.readInt32();
				r.srcAddr=await this.readInt32();
				r.callable=await this.refSlots[r.srcAddr]!.get() as PxpCallable;
				await r.callable.readParameter(r);
				this.queueRequest(r);
				break;
			case 6:
				r.destAddr=await this.readInt32();
				r.srcAddr=await this.readInt32();
				this.queueRequest(r);
				break;
			case 7:
                this.queueRequest(r);
				break;
			case 8:
				this.queueRequest(r);
				break;
            case 9:
                r.destAddr=await this.readUint32();
                this.queueRequest(r);
                break;
            case 10:
                this.queueRequest(r);
                break;
			}
		}
		this.running=false;
	}
    
	protected putRefSlots(addr:number,r:PxpObject|null) {
		if(this.refSlots[addr]!=null) 
			this.refSlots[addr]!.release();
		if(r!=null)
			r.addRef();
		this.refSlots[addr]=r;
	}
    public getStringAt(addr:number):string {
		if(this.refSlots[addr]===null) {
			return "";
		}
		let o=this.refSlots[addr]!.get();
		if(o instanceof ArrayBuffer) {
			return new TextDecoder().decode(o);
		}else {
			return o.toString();
		}
	}
	

	protected writeLock=new mutex();

	public async push(r:PxpRequest) {
		this.putRefSlots(r.destAddr,new PxpObject(r.parameter));
		await this.writeLock.mutexDo(async()=>{
            await this.writeUint32(r.session);
        });
	}
	public async pull(r:PxpRequest){
		let o:any=null;
		if(this.refSlots[r.srcAddr]!=null) {
			o=this.refSlots[r.srcAddr]!.get();
		}
        await this.writeLock.mutexDo(async ()=>{
            await this.writeUint32(r.session);
            if(o instanceof ArrayBuffer) {
                this.writeInt32(o.byteLength);
                await this.io1.write(o);
            }else if(typeof(o) === 'string'){
                let b=new TextEncoder().encode(o);
                this.writeInt32(b.byteLength);
                await this.io1.write(b);
            }else {
                await this.writeInt32(-1);
            }
        });
	}
	public async assign(r:PxpRequest) {
		this.putRefSlots(r.destAddr, this.refSlots[r.srcAddr]);
		await this.writeLock.mutexDo(async()=>{
            await this.writeUint32(r.session);
        });
	}
	public async unlink(r:PxpRequest){
		this.putRefSlots(r.destAddr, null);
		await this.writeLock.mutexDo(async()=>{
            await this.writeUint32(r.session);
        });
	}
	public async call(r:PxpRequest){
		let result=await r.callable!.call(r);
        r.result=result;
        this.putRefSlots(r.destAddr,new PxpObject(result));
        await this.writeLock.mutexDo(async()=>{
            await this.writeUint32(r.session);
            await r.callable!.writeResult(r);
        });
	}
    public funcMap:((name:string)=>PxpCallable|null)|null=null;
	public async getFunc(r:PxpRequest){
		let name=this.getStringAt(r.srcAddr);
		let found=this.funcMap?.(name);
		await this.writeLock.mutexDo(async()=>{
            if(found==null) {
                await this.writeUint32(r.session);
                await this.writeInt32(0);
            }else {
                this.putRefSlots(r.destAddr, new PxpObject(found));
                await this.writeUint32(r.session);
                await this.writeInt32(r.destAddr);
            }
        });
	}
	public async getInfo(r:PxpRequest){
		await this.writeLock.mutexDo(async()=>{
            await this.writeUint32(r.session);
            let b=new TextEncoder().encode(
            "server name:pxprpc for typescript\n"+
            "version:1.1\n"+
            "reference slots capacity:4096\n"
            );
            await this.writeInt32(b.length);
            await this.io1.write(b);
        });
	}
    public async sequence(r:PxpRequest){
        this.sequenceSession=r.destAddr
        if(this.sequenceSession==0xffffffff){
            //discard pending request. execute immdiately mode, default value
            for(let i2 in this.pendingRequests){
                let r2=this.pendingRequests[i2];
                r2.nextPending=null;
            }
            this.pendingRequests=this.pendingRequests;
        }else{
            this.sequenceMaskBitsCnt=this.sequenceSession&0xff;
            this.sequenceSession=this.sequenceSession>>(32-this.sequenceMaskBitsCnt);
        }
        await this.writeLock.mutexDo(async()=>{
            await this.writeUint32(r.session);
        });
    }
    public async buffer(r:PxpRequest){
        //Not implemented
    }

    public close(r:PxpRequest){
        this.running=false;
        for(let val of this.refSlots){
            if(val!=null)val.release();
        }
    }
}