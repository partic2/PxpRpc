from .idlcomm import IDLToBinding
from . import idldef
import typing
import sys


def convertIdlTypeToPyTypedecl(t: idldef.Type2):
    return idldef.convertIdlTypeToTypedeclChar(t)


def convertIdlTypeToTsType(t: idldef.Type2):
    if t in [idldef.Int32, idldef.Int64]:
        return 'int'
    elif t in [idldef.Float32, idldef.Float64]:
        return 'float'
    elif t == idldef.Bool:
        return 'bool'
    elif t in [idldef.Bytes]:
        return 'bytes'
    elif t in [idldef.String]:
        return 'str'
    elif isinstance(t, idldef.RemoteObjectType):
        return 'RpcExtendClientObject'
    else:
        return t.name


class PyNamespaceGenerator:
    def __init__(self, namespace: idldef.Namespace):
        self.namespace: idldef.Namespace = namespace
        self.types: typing.List[idldef.Type2] = []
        self.fns: typing.List[idldef.Function] = []

    def generateTypedefBlock(self):
        code = ["self.RemoteName='"+self.namespace.name+"';"]
        code.append('self.rpc__client:RpcExtendClient1;')
        code.append('self.rpc__RemoteFuncs={}')
        self.typedefBlock = code

    def generateInitBlock(self):
        code = ['async def useClient(self,client:RpcExtendClient1):',
                ' self.rpc__client=client',
				' self.rpc__RemoteFuncs.clear()',
                'async def ensureFunc(self,name:str,typedecl:str):',
                " remotefunc=self.rpc__RemoteFuncs.get(name,None)",
                " if(remotefunc==None):",
                "  remotefunc=await self.rpc__client.getFunc(self.RemoteName + '.'+name)",
                "  self.rpc__RemoteFuncs[name]=remotefunc",
                "  remotefunc.typedecl(typedecl)",
                " return remotefunc"]
        self.initBlock = code

    def validSymbolName(self, fnname: str):
        if fnname in ['typeof', 'async', 'await', 'import', 'export', 'class', 'def','in']:
            return fnname+'2'
        else:
            return fnname

    def generateFunctionWrap(self, fn: idldef.Function):
        code = []
        row = f'async def {self.validSymbolName(fn.name)}(self'
        row += ''.join(map(lambda p: ','+self.validSymbolName(p.name)+':' +
                       convertIdlTypeToTsType(p.type2), fn.params))
        if len(fn.results) == 0:
            row += ')->None:'
        elif len(fn.results) == 1:
            row += ')->'+convertIdlTypeToTsType(fn.results[0].type2)+':'
        else:
            row += ')->typing.Tuple['+(','.join([convertIdlTypeToTsType(t1.type2)
                                       for t1 in fn.results]))+']:'
        code.append(row)
        typedecl = ''
        for p in fn.params:
            typedecl += convertIdlTypeToPyTypedecl(p.type2)
        typedecl += '->'
        for p in fn.results:
            typedecl += convertIdlTypeToPyTypedecl(p.type2)

        code += [f" remotefunc=await self.ensureFunc('{fn.name}','{typedecl}')"]
        row = " result=await remotefunc("
        row += ','.join(map(lambda p: self.validSymbolName(p.name), fn.params))
        row += ')'
        code.append(row)
        if len(fn.results) > 0:
            code.append(' return result')
        return code

    def indentCode(self, code: typing.List[str], spaceCnt: int):
      return map(lambda t1: ' '*spaceCnt+t1, code)

    def generateFunctionsBlock(self):
        code = []
        for fn in self.fns:
            try:
                code += self.generateFunctionWrap(fn)
            except Exception as e:
                print('generate error for:'+fn.name, file=sys.stderr)
                raise e
        self.functionBlock = code

    def generate(self):
        self.preprocess()
        self.generateTypedefBlock()
        self.generateInitBlock()
        self.generateFunctionsBlock()
        nsname = self.namespace.name.replace('-', '__').replace('.', '__')
        return '\n'.join(['from pxprpc.extend import RpcExtendClientObject,RpcExtendClient1',
                          'import typing','from .pxprpc_config import getDefaultClient',
                          f"class Invoker(object):",
                          ' def __init__(self):'] +
                         list(self.indentCode(self.typedefBlock, 2)) +
                         list(self.indentCode(self.initBlock, 1)) +
                         list(self.indentCode(self.functionBlock, 1))+[
            "defaultInvoker:typing.Optional[Invoker]=None",
            "async def getDefault()->Invoker:",
            " global defaultInvoker",
            " if defaultInvoker==None:",
            "  defaultInvoker=Invoker()",
            "  await defaultInvoker.useClient(await getDefaultClient())",
            " return defaultInvoker"
        ])

    def preprocess(self):
        #categorize
        for elem in self.namespace.content:
            if isinstance(elem, idldef.Function):
                self.fns.append(elem)
            elif isinstance(elem, idldef.Type2):
                self.types.append(elem)


def generateForNamespace(namespace: idldef.Namespace):
    nsg = PyNamespaceGenerator(namespace)
    return nsg.generate()


class IDLToPyBinding(IDLToBinding):
    def toBinding(self, ns: idldef.Namespace) -> str:
        return generateForNamespace(ns)
    def fileName(self, ns: idldef.Namespace) -> str:
        return super().fileName(ns).replace('-', '__').replace('.', '__')+'.py'
    def writeConfigFile(self,dir:str):
        import os
        import io
        configFile=os.path.sep.join([dir,'pxprpc_config.py'])
        if not os.path.exists(configFile):
            with io.open(configFile,'w',encoding='utf-8') as f1:
                f1.write('''#generated by geneartor, delete this to regenerate.
from pxprpc.backend import TcpClient
from pxprpc.base import ClientContext
from pxprpc.extend import RpcExtendClient1

defaultServiceUrl='localhost:2050';
defaultClient:RpcExtendClient1=None; #type: ignore

async def getDefaultClient():
    global defaultClient
    if defaultClient==None:
        sep=defaultServiceUrl.rindex(':')
        tcpc=TcpClient(defaultServiceUrl[:sep],int(defaultServiceUrl[sep+1:]))
        rpcconn=await tcpc.start()
        defaultClient=RpcExtendClient1(rpcconn)
    return defaultClient''')