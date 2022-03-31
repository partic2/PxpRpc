

export interface Io{
    read(size:number):Promise<ArrayBuffer>;
    write(data:ArrayBufferLike):Promise<void>;
}

export class Client{
    protected nextSid1=0
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
        await this.io1.write(hdr);
        await this.io1.write(data);
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
        await this.io1.write(hdr);
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
        await this.io1.write(hdr);
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
        await this.io1.write(hdr);
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
        await this.io1.write(hdr);
        await this.io1.write(args);
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
        await this.io1.write(hdr);
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
        await this.io1.write(hdr);
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
        await this.io1.write(hdr);
        this.running=false;
    }
}