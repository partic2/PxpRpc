
import { Io } from "./base";


export class WebSocketIo implements Io{
    queuedData:Array<ArrayBufferLike>=new Array();
    protected onmsg:null|((err:Error|null)=>void)=null;
    ws:WebSocket|null=null;
    constructor(){
    }
    async wrap(ws:WebSocket){
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
    ensureConnected():Promise<this>{
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
    async connect(url:string){
        await (await this.wrap(new WebSocket(url))).ensureConnected();
        return this;
    }
    async receive(): Promise<ArrayBuffer> {
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
    async send(data: ArrayBufferLike[]): Promise<void> {
        let len=data.reduce((prev,curr)=>prev+curr.byteLength,0);
        let buf=new Uint8Array(new ArrayBuffer(len));
        let pos=0;
        for(let b of data){
            buf.set(new Uint8Array(b),pos);
            pos+=b.byteLength;
        }
        this.ws!.send(buf.buffer);
    }
    close(): void {
        this.ws?.close()
    }
}

interface BasicMessagePort{
    addEventListener:(type:'message',cb:(msg:MessageEvent)=>void)=>void
    removeEventListener:(type:'message',cb:(msg:MessageEvent)=>void)=>void
    postMessage:(data:any,opt?:{transfer:Transferable[]})=>void
}

export let WebMessage=(function(){
    //mark for pxprpc message
    const pxprpcMessageMark='__messageMark_pxprpc';
    
    let servers={} as {[id:string]:Server};
    let connections={} as {[id:string]:Connection};
    let boundList:BasicMessagePort[]=[];
    function listener(msg:MessageEvent){
        if(typeof msg.data==='object' && Boolean(msg.data[pxprpcMessageMark])){
            let type=msg.data.type as string;
            let id=msg.data.id as string;
            //for Dedicate Worker, use msg.target
            let source=msg.source??(msg.target as Worker);
            if(type==='connect'){
                let servId=msg.data.servId
                let serv=servers[servId];
                if(serv===undefined){
                    source.postMessage({[pxprpcMessageMark]:true,type:'notfound',id:id});
                }else{
                    let conn=new Connection();
                    conn.connected=true;
                    conn.id=id;
                    conn.port=source!;
                    connections[conn.id]=conn;
                    source.postMessage({[pxprpcMessageMark]:true,type:'connected',id:id});
                    serv.onConnection(conn);
                }
            }else if(type==='connected'){
                let conn=connections[id];
                if(conn===undefined){
                    source.postMessage({[pxprpcMessageMark]:true,type:'closed',id:id});
                }else{
                    //TODO: handler mutlitime connect?
                    conn.connected=true;
                    conn.port=source!;
                    conn.onmsg?.(null);
                }
            }else if(type==='notfound'){
                //TODO: Notify Connection if all bound port return notfound
            }else if(type==='data'){
                let conn=connections[id];
                if(conn===undefined){
                    source.postMessage({[pxprpcMessageMark]:true,type:'closed',id:id});
                }else{
                    conn.queuedData.push(msg.data.data);
                    conn.onmsg?.(null);
                }
            }else if(type==='closed'){
                let conn=connections[id];
                if(conn!==undefined){
                    conn.connected=false;
                    delete connections[id];
                }
            }
        }
    }
    function bind(messagePort:BasicMessagePort){
        boundList.push(messagePort);
        messagePort.addEventListener('message',listener);
    }
    function unbind(messagePort:BasicMessagePort){
        messagePort.removeEventListener('message',listener)
        let idx=boundList.findIndex(v=>v===messagePort);
        if(idx>=0){
            boundList.splice(idx,1);
        }
    }
    class Server{
        constructor(public onConnection:(conn:Connection)=>void){}
        id:string=''
        listen(id:string){
            this.id=id;
            if(servers[id]!==undefined){
                throw new Error('WebMessageServer listen failed, id already in used');
            }
            servers[id]=this;
        }
        close(){
            delete servers[this.id]
        }
    }

    class Connection implements Io{
        queuedData:Array<ArrayBufferLike>=new Array();
        onmsg:null|((err:Error|null)=>void)=null;
        port?:BasicMessagePort;
        id:string='';
        connected:boolean=false;
        constructor(){
        }
        async connect(servId:string,timeout?:number){
            this.id=(new Date().getTime()%2176782336).toString(36)+'-'+Math.floor(Math.random()*2176782336).toString(36);
            connections[this.id]=this;
            for(let port of boundList){
                port.postMessage({[pxprpcMessageMark]:true,id:this.id,type:'connect',servId})
            }
            if(!this.connected){
                await this.__waitMessage(timeout)
            }
            if(!this.connected){
                throw new Error('WebMessageConnection connect failed.');
            }
            return this;
        }
        __waitMessage(timeout?:number){
            return new Promise((resolve,reject)=>{
                this.onmsg=(err)=>{
                    if(err==null){
                        resolve(null);
                    }else{
                        reject(err);
                    }
                }
                if(timeout!=undefined){
                    setTimeout(resolve,timeout);
                }
            });
        }
        async receive(): Promise<ArrayBuffer> {
            if(!this.connected){
                throw new Error('WebMessageConnection send failed, Not connected.');
            }
            if(this.queuedData.length>0){
                return this.queuedData.shift()!;
            }else{
                await this.__waitMessage();
                return this.queuedData.shift()!;
            }
        }
        async send(data: ArrayBufferLike[]): Promise<void> {
            if(!this.connected){
                throw new Error('WebMessageConnection send failed, Not connected.');
            }
            let len=data.reduce((prev,curr)=>prev+curr.byteLength,0);
            let buf=new Uint8Array(new ArrayBuffer(len));
            let pos=0;
            for(let b of data){
                buf.set(new Uint8Array(b),pos);
                pos+=b.byteLength;
            }
            this.port!.postMessage({[pxprpcMessageMark]:true,id:this.id,type:'data',data:buf.buffer},{transfer:[buf.buffer]});
        }
        close(): void {
            this.port!.postMessage({[pxprpcMessageMark]:true,type:'closed',id:this.id})
            this.connected=false;
            delete connections[this.id];
        }
    }
    return {bind,unbind,Server,Connection}
})()
