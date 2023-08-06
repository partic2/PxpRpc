

export interface Io{
    read(size:number):Promise<ArrayBuffer>;
    write(data:ArrayBufferLike):Promise<void>;
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
    protected async nextSid(opcode:number){
        let sid=opcode|this.nextSid1;
        this.nextSid1+=0x100;
        return sid;
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
    public async push(destAddr:number,data:ArrayBufferLike){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(1);
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
    public async pull(srcAddr:number){
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(2);
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
    public async assign(destAddr:number,srcAddr:number){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(3);
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
    public async unlink(destAddr:number){
        let hdr=new ArrayBuffer(8);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(4);
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
    public async call(destAddr:number,fnAddr:number,args:ArrayBufferLike,sizeOfReturn:number){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(5);
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
            await this.io1.write(args);
        });
        await respFut;
        let data1=await this.io1.read(sizeOfReturn);
        this.respReadingCb(null);
        return data1
    }
    public async getFunc(destAddr:number,fnNameAddr:number){
        let hdr=new ArrayBuffer(12);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(6);
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
    public async getInfo(){
        let hdr=new ArrayBuffer(4);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(8);
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
    public async close(){
        let hdr=new ArrayBuffer(4);
        let hdr2=new DataView(hdr);
        let sid=await this.nextSid(6);
        hdr2.setUint32(0,sid,true);
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(hdr);
        });
        this.running=false;
    }
}

export class PxpRequest{
    public destAddr=0;
    public srcAddr=0;
    public funcAddr=0;
    public parameter:any;
    public result:any;
    public callable:PxpCallable|null=null;
    public session:ArrayBufferLike|null=null;
    public opcode:number=0;
    public constructor(public context:Server){

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
    public async serve(){
        this.running=true;
		while(this.running) {
			let session=await this.io1.read(4);
            let r=new PxpRequest(this);
			r.session=session;
			r.opcode=new DataView(session).getUint8(0);
			switch(r.opcode) {
			case 1:
				r.destAddr=await this.readInt32();
				let len=await this.readInt32();
				r.parameter=await this.io1.read(len);
				this.push(r);
				break;
			case 2:
				r.srcAddr=await this.readInt32();
				this.pull(r);
				break;
			case 3:
				r.destAddr=await this.readInt32();
				r.srcAddr=await this.readInt32();
				this.assign(r);
				break;
			case 4:
				r.destAddr=await this.readInt32();
				this.unlink(r);
				break;
			case 5:
				r.destAddr=await this.readInt32();
				r.srcAddr=await this.readInt32();
				r.callable=await this.refSlots[r.srcAddr]!.get() as PxpCallable;
				await r.callable.readParameter(r);
				this.call(r);
				break;
			case 6:
				r.destAddr=await this.readInt32();
				r.srcAddr=await this.readInt32();
				this.getFunc(r);
				break;
			case 7:
				close();
				this.running=false;
				break;
			case 8:
				this.getInfo(r);
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
            await this.io1.write(r.session!);
        });
	}
	public async pull(r:PxpRequest){
		let o:any=null;
		if(this.refSlots[r.srcAddr]!=null) {
			o=this.refSlots[r.srcAddr]!.get();
		}
        await this.writeLock.mutexDo(async ()=>{
            await this.io1.write(r.session!);
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
            await this.io1.write(r.session!);
        });
	}
	public async unlink(r:PxpRequest){
		this.putRefSlots(r.destAddr, null);
		await this.writeLock.mutexDo(async()=>{
            await this.io1.write(r.session!);
        });
	}
	public async call(r:PxpRequest){
		let result=await r.callable!.call(r);
        r.result=result;
        this.putRefSlots(r.destAddr,new PxpObject(result));
        await this.writeLock.mutexDo(async()=>{
            await this.io1.write(r.session!);
            await r.callable!.writeResult(r);
        });
	}
    public funcMap:((name:string)=>PxpCallable|null)|null=null;
	public async getFunc(r:PxpRequest){
		let name=this.getStringAt(r.srcAddr);
		let found=this.funcMap?.(name);
		await this.writeLock.mutexDo(async()=>{
            if(found==null) {
                await this.io1.write(r.session!);
                await this.writeInt32(0);
            }else {
                this.putRefSlots(r.destAddr, new PxpObject(found));
                await this.io1.write(r.session!);
                await this.writeInt32(r.destAddr);
            }
        });
	}
	public async getInfo(r:PxpRequest){
		await this.writeLock.mutexDo(async()=>{
            await this.io1.write(r.session!);
            let b=new TextEncoder().encode(
            "server name:pxprpc for typescript\n"+
            "version:1.0\n"+
            "reference slots capacity:"+this.refSlots.length+"\n"
            );
            await this.writeInt32(b.length);
            await this.io1.write(b);
        });
	}
}