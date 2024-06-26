from . import idldef
import typing
import sys
from .idlcomm import IDLToBinding


def convertIdlTypeToTsTypedecl(t:idldef.Type2):
    return idldef.convertIdlTypeToTypedeclChar(t)

def convertIdlTypeToTsType(t:idldef.Type2):
    if t in [idldef.Int32,idldef.Float32,idldef.Float64]:
        return 'number'
    elif t==idldef.Int64:
        return 'BigInt'
    elif t==idldef.Bool:
        return 'boolean'
    elif t in [idldef.Bytes]:
        return 'Uint8Array'
    elif t in [idldef.String]:
        return 'string'
    elif isinstance(t,idldef.RemoteObjectType):
        return 'RpcExtendClientObject'
    else:
        return t.name


class TsNamespaceGenerator:
    def __init__(self,namespace:idldef.Namespace):
        self.namespace:idldef.Namespace=namespace
        self.types:typing.List[idldef.Type2]=[]
        self.fns:typing.List[idldef.Function]=[]
        
    def generateTypedefBlock(self):
        code=["RemoteName='"+self.namespace.name+"';"]
        code.append('rpc__client?:RpcExtendClient1;')
        code.append('rpc__RemoteFuncs={} as {[k:string]:RpcExtendClientCallable|undefined|null};')
        self.typedefBlock=code

    def generateInitBlock(self):
        code=['async useClient(client:RpcExtendClient1){']
        code+=[' this.rpc__client=client;',
        ' this.rpc__RemoteFuncs={}',
        '}','async ensureFunc(name:string,typedecl:string){',
        ' let __v1=this.rpc__RemoteFuncs[name];',
        ' if(__v1==undefined){',
        "  __v1=await this.rpc__client!.getFunc(this.RemoteName + '.' + name);",
        "  this.rpc__RemoteFuncs[name]=__v1",
        "  __v1!.typedecl(typedecl);",
        " }"," return __v1;",'}']
        self.initBlock=code

    def validSymbolName(self,fnname:str):
        if fnname in ['typeof','async','await','import','export','class','in','try']:
            return fnname+'2'
        else:
            return fnname

    def generateFunctionWrap(self,fn:idldef.Function):
        code=[]
        row=f'async {self.validSymbolName(fn.name)}('
        row+=','.join(map(lambda p:self.validSymbolName(p.name)+':'+convertIdlTypeToTsType(p.type2),fn.params))
        if len(fn.results)==0:
            row+='):Promise<void>{'
        elif len(fn.results)==1:
            row+='):Promise<'+convertIdlTypeToTsType(fn.results[0].type2)+'>{'
        else:
            row+='):Promise<['+(','.join([convertIdlTypeToTsType(t1.type2) for t1 in fn.results]))+']>{'
        code.append(row)
        typedecl=''
        for p in fn.params:
            typedecl+=convertIdlTypeToTsTypedecl(p.type2)
        typedecl+='->'
        for p in fn.results:
            typedecl+=convertIdlTypeToTsTypedecl(p.type2)
        code+=[f" let __v1=await this.ensureFunc('{fn.name}','{typedecl}');"]
        row=f" let __v2=await __v1!.call("
        row+=','.join(map(lambda p:self.validSymbolName(p.name),fn.params))
        if len(fn.results)==0:
            row+=');'
        else:
            row+=') as any;'
        code.append(row)

        if len(fn.results)>0:
            code.append(' return __v2;')
            
        code.append('}')
        return code

    def generateFunctionsBlock(self):
        code=[]
        for fn in self.fns:
            try:
                code+=self.generateFunctionWrap(fn)
            except Exception as e:
                print('generate error for:'+fn.name,file=sys.stderr)
                raise e
        self.functionBlock=code

    def indentCode(self,code:typing.List[str],spaceCnt:int):
      return map(lambda t1:' '*spaceCnt+t1,code)

    def generate(self):
        self.preprocess()
        self.generateTypedefBlock()
        self.generateInitBlock()
        self.generateFunctionsBlock()
        nsname=self.namespace.name.replace('-','__').replace('.','__')
        header=[]
        header.append("import {RpcExtendClient1,RpcExtendClientCallable,RpcExtendClientObject} from 'pxprpc/extend'")
        header.append("import { getDefaultClient } from './pxprpc_config';")
        return '\n'.join(header+[
        f"export class Invoker{{"]+
        list(self.indentCode(self.typedefBlock,1))+
        list(self.indentCode(self.initBlock,1))+
        list(self.indentCode(self.functionBlock,1))+
        ['}',\
        '''let defaultInvoker:Invoker|null=null;
export async function getDefault(){
 if(defaultInvoker===null){
  defaultInvoker=new Invoker();
  await defaultInvoker.useClient(await getDefaultClient());
  }
 return defaultInvoker;
}'''])

        
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


class IDLToTsBinding(IDLToBinding):
    def toBinding(self, ns: idldef.Namespace) -> str:
        return generateForNamespace(ns)
    def fileName(self, ns: idldef.Namespace) -> str:
        return super().fileName(ns).replace('-', '__').replace('.', '__')+'.ts'
    def writeConfigFile(self,dir:str):
        import os
        import io
        configFile=os.path.sep.join([dir,'pxprpc_config.ts'])
        if not os.path.exists(configFile):
            with io.open(configFile,'w',encoding='utf-8') as f1:
                f1.write('''//generated by geneartor, delete this to regenerate.
import { WebSocketIo } from "pxprpc/backend";
import { Client } from "pxprpc/base";
import { RpcExtendClient1 } from "pxprpc/extend";

export var defaultServiceUrl='ws://'+location.host+'/pxprpc/2050';
export var defaultClient:RpcExtendClient1|null=null;

export async function getDefaultClient(){
    if(defaultClient===null){
        defaultClient=await new RpcExtendClient1(
            new Client(await new WebSocketIo().connect(defaultServiceUrl))).init();
    }
    return defaultClient;
}''')