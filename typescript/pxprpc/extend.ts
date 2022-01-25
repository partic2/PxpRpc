import { Client } from './base'

export class RpcExtendError extends Error {
}

export class RpcExtendClientObject {
    public constructor(public client: RpcExtendClient1, public value: number | undefined) {
    }
    public async tryPull() {
        return this.client.conn.pull(this.value!);
    }
    public async free() {
        if (this.value != undefined) {
            await this.client.freeSlot(this.value);
        }
    }

}

export class RpcExtendClientCallable extends RpcExtendClientObject {
    protected sign: string = '->';
    public constructor(client: RpcExtendClient1, value: number | undefined) {
        super(client, value)
    }
    /*
function signature
format: 'parameters type->return type' 
eg:
a function defined in c:
bool fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
defined in java:
boolean fn(int,int,double,Object)
...
it's pxprpc signature: 
iido->z

available type signature characters:
i  int(32bit integer)
l  long(64bit integer) 
f  float(32bit float)
d  double(64bit float)
o  object(32bit reference address)
b  bytes(32bit address refer to a bytes buffer)
v  void(32bit 0)

z  boolean(pxprpc use 32bit to store boolean value)
s  string(bytes will be decode to string)
*/
    public signature(sign: string) {
        this.sign = sign;
    }

    public async call(...args:any[]) {
        let sign2 = this.sign
        let argSign = sign2.substring(0, sign2.lastIndexOf('->'))
        let retType = sign2.substring(argSign.length + 2)
        retType = retType == '' ? 'v' : retType;

        let freeBeforeReturn: number[] = []
        //TODO: fix slightly memory waste.
        let args2 = new DataView(new ArrayBuffer(args.length * 8));
        let writeAt = 0;
        try {
            let argsProcessDone = false;
            for (let i1 = 0; i1 < argSign.length; i1++) {
                switch (argSign.charAt(i1)) {
                    case 'i':
                        args2.setInt32(writeAt, args[i1], true);
                        writeAt += 4;
                        break;
                    case 'l':
                        args2.setBigInt64(writeAt, args[i1], true);
                        writeAt += 8;
                        break;
                    case 'f':
                        args2.setFloat32(writeAt, args[i1], true);
                        writeAt += 4;
                        break;
                    case 'd':
                        args2.setFloat64(writeAt, args[i1], true);
                        writeAt += 8;
                        break;
                    case 'o':
                        args2.setInt32(writeAt, args[i1].value, true);
                        writeAt += 4;
                        break;
                    case 'v':
                        throw new RpcExtendError('Unsupport input argument')
                    case 's':
                    case 'b':
                        let t2 = await this.client.allocSlot()
                        freeBeforeReturn.push(t2)
                        if (argSign.charAt(i1) == 's') {
                            await this.client.conn.push(t2, new TextEncoder().encode(args[i1]))
                        } else {
                            await this.client.conn.push(t2, args[i1])
                        }
                        args2.setInt32(writeAt, t2, true);
                        writeAt += 4;
                        break;
                    case 'z':
                        args2.setInt32(writeAt, args[i1] ? 1 : 0, true);
                        writeAt += 4;
                        break;
                }
                if (argsProcessDone) {
                    break;
                }
            }
            if ('ilfdvz'.indexOf(retType) >= 0) {
                let result = new DataView(await this.client.conn.call(
                    0, this.value!, args2.buffer.slice(0, writeAt),
                    'ld'.indexOf(retType) >= 0 ? 6 : 4));
                let result2 = null;
                switch (retType) {
                    case 'i':
                        result2 = result.getInt32(0, true);
                        break;
                    case 'l':
                        result2 = result.getBigInt64(0, true);
                        break;
                    case 'f':
                        result2 = result.getFloat32(0, true);
                        break;
                    case 'd':
                        result2 = result.getFloat64(0, true);
                        break;
                    case 'z':
                        result2 = result.getInt32(0, true) != 0;
                        break;
                }
                return result2
            }else{
                let t1=await this.client.allocSlot()
                let result = new DataView(await this.client.conn.call(
                    t1, this.value!, args2.buffer.slice(0, writeAt), 4));

                if(retType=='s'){
                    let t2=new TextDecoder().decode(await this.client.conn.pull(t1));
                    await this.client.freeSlot(t1);
                    return t2
                }else if(retType=='b'){
                    let t2=await this.client.conn.pull(t1)
                    await this.client.freeSlot(t1)
                    return t2
                }else{
                    return new RpcExtendClientObject(this.client,t1)
                }
            }
        } finally {
            for (let t1 of freeBeforeReturn) {
                await this.client.freeSlot(t1);
            }
        }

    }

}



export class RpcExtendClient1 {
    private __usedSlots: { [index: number]: boolean | undefined } = {};
    private __nextSlots: number;

    private __slotStart: number = 1;
    private __slotEnd: number = 64;
    public constructor(public conn: Client) {
        this.__nextSlots = this.__slotEnd;
    }
    public async allocSlot() {
        let reachEnd = false;

        while (this.__usedSlots[this.__nextSlots]) {
            this.__nextSlots += 1
            if (this.__nextSlots >= this.__slotEnd) {
                if (reachEnd) {
                    throw new RpcExtendError('No slot available')
                } else {
                    reachEnd = true
                    this.__nextSlots = this.__slotStart
                }
            }
        }

        let t1 = this.__nextSlots
        this.__nextSlots += 1
        this.__usedSlots[t1] = true;
        return t1
    }
    public async freeSlot(index: number) {
        if (this.conn.isRunning()) {
            await this.conn.unlink(index)
            delete this.__usedSlots[index]
        }

    }
    public async getFunc(name: string) {
        let t1 = await this.allocSlot()
        await this.conn.push(t1, new TextEncoder().encode(name))
        let t2 = await this.allocSlot()
        await this.conn.getFunc(t2, t1)
        await this.freeSlot(t1)
        return new RpcExtendClientCallable(this, t2)
    }

}




