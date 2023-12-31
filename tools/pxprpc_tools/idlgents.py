from . import idldef
import typing
import sys



def convertIdlTypeToTsSig(t:idldef.Type2):
    if t==idldef.Int32:
        return 'i'
    elif t==idldef.Int64:
        return 'l'
    elif t==idldef.Float32:
        return 'f'
    elif t==idldef.Float64:
        return 'd'
    elif t in [idldef.RemoteBytes,idldef.RemoteObject]:
        return 'o'
    elif t in [idldef.String]:
        return 's'
    elif t in [idldef.Bytes]:
        return 'b'
    elif t in [idldef.Bool]:
        return 'z'
    elif isinstance(t,(idldef.DataObjectType,idldef.DataObjectArrayType)):
        return 'b'
    else:
        return 'o'

def convertIdlTypeToTsType(t:idldef.Type2):
    if t in [idldef.Int32,idldef.Float32,idldef.Float64]:
        return 'number'
    elif t==idldef.Int64:
        return 'BigInt'
    elif t==idldef.Bool:
        return 'boolean'
    elif t in [idldef.Bytes]:
        return 'ArrayBufferLike'
    elif t in [idldef.RemoteObject,idldef.RemoteBytes]:
        return 'RpcExtendClientObject'
    else:
        return t.name


class TsNamespaceGenerator:
    def __init__(self,namespace:idldef.Namespace):
        self.namespace:idldef.Namespace=namespace
        self.types:typing.List[idldef.Type2]=[]
        self.fns:typing.List[idldef.Function]=[]
        
    def generateTypedefBlock(self):
        code="var RemoteName='"+self.namespace.name+"';\n"
        code+='var rpc__client:RpcExtendClient1;\n'
        code+='var rpc__RemoteFuncs={} as {[k:string]:RpcExtendClientCallable|undefined|null};\n'
        for type2 in self.types:
            code+='type '+type2.name+'=RpcExtendClientObject;\n'
            code+='var rpc__RemoteFuncs={} as {[k:string]:RpcExtendClientCallable};\n'
        self.typedefBlock=code

    def generateInitBlock(self):
        code='''export async function rpc__connect(){
        rpc__client=await new RpcExtendClient1(new Client(await new WebSocketIo().connect(serverUrl))).init();\n'''
        code+='}\n'
        self.initBlock=code

    def validFunctionName(self,fnname:str):
        if fnname in ['typeof','async','await','import','export','class']:
            return fnname+'2'
        else:
            return fnname

    def generateFunctionWrap(self,fn:idldef.Function):
        code=f'''export async function {self.validFunctionName(fn.name)}('''
        for p in fn.params:
            code+=p.name+':'
            code+=convertIdlTypeToTsType(p.type2)
            code+=','
        # remove ','
        if code[-1]==',':
            code=code[:-1]
        code+='):Promise<'+('void' if fn.result==None else convertIdlTypeToTsType(fn.result.type2))+'>{\n'
        sigstr=''
        for p in fn.params:
            sigstr+=convertIdlTypeToTsSig(p.type2)
        sigstr+='->'
        r=fn.result
        if r!=None:
            sigstr+=convertIdlTypeToTsSig(r.type2)
        code+=f'''let remotefunc=rpc__RemoteFuncs.{fn.name};
        if(remotefunc==undefined){{
            remotefunc=await rpc__client.getFunc(RemoteName + '.{fn.name}');
            rpc__RemoteFuncs.{fn.name}=remotefunc
            remotefunc!.typedecl('{sigstr}');
        }}
        '''
        code+=f"let result=await remotefunc!.call("
        for p in fn.params:
            code+=p.name
            code+=','
        if code[-1]==',':
            code=code[:-1]
        if fn.result==None:
            code+=');\n'
        elif isinstance(fn.result.type2,(idldef.DataObjectType,idldef.DataObjectArrayType)):
            code+=') as '+convertIdlTypeToTsType(idldef.Bytes)+';\n'
        else:
            code+=') as '+convertIdlTypeToTsType(fn.result.type2)+';\n'
        
        if fn.result==None:
            pass
        elif isinstance(fn.result.type2,(idldef.DataObjectType,idldef.DataObjectArrayType)):
            code+='return msgpack.decode(result) as '+convertIdlTypeToTsType(fn.result.type2)+';\n'
        else:
            code+='return result;\n'
 
        code+='}\n'
        return code

    def generateFunctionsBlock(self):
        code=''
        for fn in self.fns:
            try:
                code+=self.generateFunctionWrap(fn)
            except Exception as e:
                print('generate error for:'+fn.name,file=sys.stderr)
                raise e
        self.functionBlock=code

    def generate(self):
        self.preprocess()
        self.generateTypedefBlock()
        self.generateInitBlock()
        self.generateFunctionsBlock()
        return 'export namespace '+self.namespace.name.replace('-','__')+\
          '{\n'+self.typedefBlock+self.initBlock+self.functionBlock+'\n}'

        
    def preprocess(self):
        #categorize
        for elem in self.namespace.content:
            if isinstance(elem,idldef.Function):
                self.fns.append(elem)
            elif isinstance(elem,idldef.Type2):
                self.types.append(elem)

        

def generateForNamespace(namespace:idldef.Namespace):
    nsg=TsNamespaceGenerator(namespace)
    return nsg.generate()