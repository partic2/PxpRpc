
import { Io } from "./base";


export class WebSocketIo implements Io{
    public queuedData:Array<ArrayBufferLike>=new Array();
    protected onmsg:null|((err:Error|null)=>void)=null;
    public ws:WebSocket|null=null;
    public constructor(){
    }
    public async wrap(ws:WebSocket){
        this.ws=ws;
        var that=this;
        this.ws.binaryType='arraybuffer'
        ws.addEventListener('message',function(ev){
            that.queuedData.push(ev.data as ArrayBuffer)
            if(that.onmsg!=null)that.onmsg(null);
        });
        ws.addEventListener('close',function(ev){
            if(that.onmsg!=null)that.onmsg(new Error('Websocket EOF'));
        });
        ws.addEventListener('error',function(ev){
            ws.close();
            if(that.onmsg!=null)that.onmsg(new Error(String(ev)));
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
    public async receive(): Promise<ArrayBuffer> {
        if(this.queuedData.length>0){
            return this.queuedData.shift()!;
        }else{
            let that=this;
            await new Promise((resolve,reject)=>{
                that.onmsg=(err)=>{
                    if(err==null){
                        resolve(null);
                    }else{
                        reject(err);
                    }
                }
            });
            return this.queuedData.shift()!;
        }
    }
    public async send(data: ArrayBufferLike[]): Promise<void> {
        let len=data.reduce((prev,curr)=>prev+curr.byteLength,0);
        let buf=new Uint8Array(new ArrayBuffer(len));
        let pos=0;
        for(let b of data){
            buf.set(new Uint8Array(b),pos);
            pos+=b.byteLength;
        }
        this.ws!.send(buf.buffer);
    }
    public close(): void {
        this.ws?.close()
    }
}