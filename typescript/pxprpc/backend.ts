
import { Io } from "./base";


export class WebSocketIo implements Io{
    queuedData:Array<Uint8Array>=new Array();
    protected onmsg:null|((err:Error|null)=>void)=null;
    ws:WebSocket|null=null;
    constructor(){
    }
    async wrap(ws:WebSocket){
        this.ws=ws;
        var that=this;
        this.ws.binaryType='arraybuffer'
        ws.addEventListener('message',function(ev){
            that.queuedData.push(new Uint8Array(ev.data as ArrayBuffer))
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
            if(that.ws!.readyState===WebSocket.CONNECTING){
                that.ws!.addEventListener('open',function(ev){
                    resolve(that);
                });
                let onerr=function(ev:Event){
                    that.ws!.removeEventListener('error',onerr);
                    reject(new Error('WebSocket error'))
                }
                that.ws!.addEventListener('error',onerr)
            }else if(that.ws!.readyState===WebSocket.OPEN){
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
    async receive(): Promise<Uint8Array> {
        if(this.ws?.readyState!=WebSocket.OPEN){
            throw new Error('Illegal websocket readState '+String(this.ws?.readyState));
        }
        if(this.queuedData.length>0){
            return this.queuedData.shift()!;
        }else{
            let that=this;
            await new Promise((resolve,reject)=>{
                that.onmsg=(err)=>{
                    that.onmsg=null;
                    if(err===null){
                        resolve(null);
                    }else{
                        reject(err);
                    }
                }
            });
            return this.queuedData.shift()!;
        }
    }
    async send(data: Uint8Array[]): Promise<void> {
        if(this.ws?.readyState!=WebSocket.OPEN){
            throw new Error('Illegal websocket readState '+String(this.ws?.readyState));
        }
        let len=data.reduce((prev,curr)=>prev+curr.byteLength,0);
        let buf=new Uint8Array(len);
        let pos=0;
        for(let b of data){
            buf.set(new Uint8Array(b.buffer,b.byteOffset,b.byteLength),pos);
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
    postMessage:(data:any,opt?:{transfer?:Transferable[]})=>void
}

export let WebMessage=(function(){
    //mark for pxprpc message
    const pxprpcMessageMark='__messageMark_pxprpc';
    //extra options for postMessage, like 'targetOrigin'
    //Take care for security risk.
    const postMessageOptions:Record<string,any>={};
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
                    source.postMessage({[pxprpcMessageMark]:true,type:'notfound',id:id},postMessageOptions);
                }else{
                    let conn=new Connection();
                    conn.connected=true;
                    conn.id=id;
                    conn.port=source!;
                    connections[conn.id]=conn;
                    source.postMessage({[pxprpcMessageMark]:true,type:'connected',id:id},postMessageOptions);
                    serv.onConnection(conn);
                }
            }else if(type==='connected'){
                let conn=connections[id];
                if(conn===undefined){
                    source.postMessage({[pxprpcMessageMark]:true,type:'closed',id:id},postMessageOptions);
                }else{
                    //Only handle the first 'connected' event.
                    if(!conn.connected){
                        conn.connected=true;
                        conn.port=source!;
                        conn.onmsg?.(null);
                    }
                }
            }else if(type==='notfound'){
                let conn=connections[id];
                if(conn!==undefined){
                    conn.__notfoundResponse();
                }
            }else if(type==='data'){
                let conn=connections[id];
                if(conn===undefined){
                    source.postMessage({[pxprpcMessageMark]:true,type:'closed',id:id},postMessageOptions);
                }else{
                    conn.queuedData.push(new Uint8Array(msg.data.data as ArrayBuffer));
                    conn.onmsg?.(null);
                }
            }else if(type==='closed'){
                let conn=connections[id];
                if(conn!==undefined){
                    conn.connected=false;
                    delete connections[id];
                    conn.onmsg?.(new Error('WebMessageConnection closed'));
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
        queuedData:Array<Uint8Array>=new Array();
        onmsg:null|((err:Error|null)=>void)=null;
        port?:BasicMessagePort;
        id:string='';
        connected:boolean=false;
        __broadcastCount=0;
        __notfoundCount=0;
        constructor(){
        }
        async connect(servId:string,timeout?:number){
            this.id=(new Date().getTime()%2176782336).toString(36)+'-'+Math.floor(Math.random()*2176782336).toString(36);
            connections[this.id]=this;
            try{
                this.__broadcastCount=boundList.length;
                if(this.__broadcastCount===0){throw new Error('WebMessageConnection connect failed. server not found.')}
                for(let port of boundList){
                    port.postMessage({[pxprpcMessageMark]:true,id:this.id,type:'connect',servId},{transfer:[],...postMessageOptions})
                }
                if(!this.connected){
                    await this.__waitMessage(timeout)
                }
                if(!this.connected){
                    throw new Error('WebMessageConnection connect failed. timeout. server not found.');
                }
                return this;
            }catch(e){
                delete connections[this.id];
                throw e;
            }
        }
        __waitMessage(timeout?:number){
            return new Promise((resolve,reject)=>{
                let timer1: any=null;
                if(timeout!=undefined){
                    timer1=setTimeout(()=>{
                        this.onmsg=null;
                        resolve(null);
                    },timeout);
                }
                this.onmsg=(err)=>{
                    if(timer1!=null){
                        clearTimeout(timer1);
                    }
                    this.onmsg=null;
                    if(err===null){
                        resolve(null);
                    }else{
                        reject(err);
                    }
                    
                }
                
            });
        }
        __notfoundResponse(){
            this.__notfoundCount++;
            if(!this.connected && this.__notfoundCount>=this.__broadcastCount){
                this.onmsg?.(new Error('WebMessageConnection connect failed. server not found.'))
            }
        }
        async receive(): Promise<Uint8Array> {
            if(!this.connected){
                throw new Error('WebMessageConnection receive failed, Not connected.');
            }
            if(this.queuedData.length>0){
                return this.queuedData.shift()!;
            }else{
                await this.__waitMessage();
                return this.queuedData.shift()!;
            }
        }
        async send(data: Uint8Array[]): Promise<void> {
            if(!this.connected){
                throw new Error('WebMessageConnection send failed, Not connected.');
            }
            let len=data.reduce((prev,curr)=>prev+curr.byteLength,0);
            let buf=new Uint8Array(len);
            let pos=0;
            for(let b of data){
                buf.set(new Uint8Array(b.buffer,b.byteOffset,b.byteLength),pos);
                pos+=b.byteLength;
            }
            this.port!.postMessage({[pxprpcMessageMark]:true,id:this.id,type:'data',data:buf.buffer},{transfer:[buf.buffer],...postMessageOptions});
        }
        close(): void {
            this.port!.postMessage({[pxprpcMessageMark]:true,type:'closed',id:this.id},postMessageOptions)
            this.connected=false;
            delete connections[this.id];
        }
    }
    return {bind,unbind,Server,Connection,postMessageOptions}
})()
