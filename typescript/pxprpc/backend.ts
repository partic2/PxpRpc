import { Client, Io } from "./base";



export class WebSocketIo implements Io{
    public queuedData:Array<ArrayBufferLike>=new Array();
    protected onmsg:null|(()=>void)=null;
    public constructor(public ws:WebSocket){
        var that=this;
        ws.addEventListener('message',function(ev){
            that.queuedData.push(ev.data as ArrayBuffer)
            if(that.onmsg!=null)that.onmsg();
        });
    };
    public async waitNewMessage(){
        return new Promise((resolve)=>{
            this.onmsg=()=>{
                this.onmsg=null;
                resolve(null);
            }
        });
    }
    async read(size: number): Promise<ArrayBuffer> {
        let remain=size;
        let datar=new Uint8Array(new ArrayBuffer(size));
        while(remain>0){
            let data1=this.queuedData.shift()
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
    async write(data: ArrayBufferLike): Promise<void> {
        this.ws.send(data);
    }

}
export class PxprpcWebSocketClient{
    protected ws:WebSocket|undefined
    protected client?:Client
    public async connect(url:string){
        this.ws=new WebSocket(url);
        this.ws!.binaryType='arraybuffer';
        return new Promise((resolve)=>{
            this.ws!.addEventListener('open',(ev)=>{
                this.client=new Client(new WebSocketIo(this.ws!));
                resolve(null);
            });
        });
    }
    public async wrapWebSocket(ws:WebSocket){
        this.ws=ws;
        this.ws!.binaryType='arraybuffer';
    }
    public async run(){
        await this.client!.run()
    }
    public async close(){
        this.client!.close()
    }
    public rpc(){
        return this.client;
    }
}