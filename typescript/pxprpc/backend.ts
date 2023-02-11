
import { Client, Io } from "./base";
import { RpcExtendError } from "./extend";



export class WebSocketIo implements Io{
    public queuedData:Array<ArrayBufferLike>=new Array();
    protected onmsg:null|(()=>void)=null;
    public ws:WebSocket|null=null;
    public constructor(){
    };
    public async wrap(ws:WebSocket){
        this.ws=ws;
        var that=this;
        this.ws.binaryType='arraybuffer'
        ws.addEventListener('message',function(ev){
            that.queuedData.push(ev.data as ArrayBuffer)
            if(that.onmsg!=null)that.onmsg();
        });
        ws.addEventListener('close',function(ev){
            if(that.onmsg!=null)that.onmsg();
        });
        ws.addEventListener('error',function(ev){
            ws.close();
            if(that.onmsg!=null)that.onmsg();
        })
        return this;
    }
    public ensureConnected():Promise<this>{
        let that=this;
        return new Promise<this>((resolve,reject)=>{
            if(that.ws!.readyState==WebSocket.CONNECTING){
                that.ws!.addEventListener('open',function(ev){
                    resolve(that);
                });
                let onerr=function(ev:Event){
                    that.ws!.removeEventListener('error',onerr);
                    reject(new Error('WebSocket error'))
                }
                that.ws!.addEventListener('error',onerr)
            }else if(that.ws!.readyState==WebSocket.OPEN){
                resolve(that);
            }else{
                reject(new Error('Invalid WebSocket readyState:'+this.ws!.readyState))
            }
        });
    }
    public async connect(url:string){
        await (await this.wrap(new WebSocket(url))).ensureConnected();
        return this;
    }
    public async waitNewMessage(){
        return new Promise((resolve)=>{
            this.onmsg=()=>{
                this.onmsg=null;
                resolve(null);
            }
        });
    }
    public availableBytes(){
        let sumBytes=0;
        for(let i1=0;i1<this.queuedData.length;i1++){
            sumBytes+=this.queuedData[i1].byteLength;
        }
        return sumBytes;
    }
    public async read(size: number): Promise<ArrayBuffer> {
        let remain=size;
        let datar=new Uint8Array(new ArrayBuffer(size));
        while(remain>0){
            if(this.ws!.readyState!=WebSocket.OPEN){
                throw new RpcExtendError('WebSocket EOF')
            }
            let data1=this.queuedData.shift();
            if(data1==undefined){
                await this.waitNewMessage();
                continue;
            }
            if(data1.byteLength<=remain){
                datar.set(new Uint8Array(data1),size-remain);
                remain-=data1.byteLength;
            }else{
                datar.set(new Uint8Array(data1,0,remain),size-remain)
                data1=data1.slice(remain)
                this.queuedData.unshift(data1)
                remain=0;
            }
        }
        return datar.buffer;
    }
    public async write(data: ArrayBufferLike): Promise<void> {
        this.ws!.send(data);
    }

}