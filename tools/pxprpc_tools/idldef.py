import typing

        
class Type2:
    def __init__(self,name:str):
        self.name=name

class RemoteObjectType(Type2):
    def __init__(self,remoteName:str):
        super().__init__('RemoteObject')
        self.remoteName=remoteName

class TypedVar:
    def __init__(self,type2:Type2,name:str):
        self.type2=type2
        self.name=name
        
        
        
class Function:
    def __init__(self,name:str):
        self.name=name
        self.params:typing.List[TypedVar]=[]
        self.results:typing.List[TypedVar]=[]

    def setParams(self,*typeValuePair)->'Function':
        for t1 in range(0,len(typeValuePair),2):
            self.params.append(TypedVar(typeValuePair[t1],typeValuePair[t1+1]))
        return self

    def setResults(self,*typeValuePair)->'Function':
        for t1 in range(0,len(typeValuePair),2):
            self.results.append(TypedVar(typeValuePair[t1],typeValuePair[t1+1]))
        return self
        
class Namespace:
    def __init__(self,name:str):
        self.name=name
        self.content=[]

    def addType(self,t:Type2):
        self.content.append(t)
        return self

    def addFunc(self,fn:Function):
        self.content.append(fn)
        return self

Int32=Type2('Int32')
Int64=Type2('Int64')
Float32=Type2('Float32')
Float64=Type2('Float64')
RemoteObject=RemoteObjectType('Any')
Bytes=Type2('Bytes')
Bool=Type2('Bool')
String=Type2('String')



def TypesFromTypedeclString(typedecl:str):
    typemap={
        'b':Bytes,
        'i':Int32,
        'l':Int64,
        'f':Float32,
        'd':Float64,
        's':String,
        'c':Bool,
        'o':RemoteObject
    }
    return [typemap[t1] for t1 in typedecl]

def convertIdlTypeToTypedeclChar(t:Type2):
    if t==Int32:
        return 'i'
    elif t==Int64:
        return 'l'
    elif t==Float32:
        return 'f'
    elif t==Float64:
        return 'd'
    elif isinstance(t,RemoteObjectType):
        return 'o'
    elif t==String:
        return 's'
    elif t==Bytes:
        return 'b'
    elif t==Bool:
        return 'c'
    else:
        return 'o'

def GenIdlDefNamespaceScript(ns:Namespace):
    defstr=['from pxprpc_tools.idldef import *',\
        "ns=Namespace('{0}')".format(ns.name)]
    for item in ns.content:
        if type(item)==Function:
            item=typing.cast(Function,item)
            defstr.append("rfn=Function('{0}')\\\n".format(item.name)+
            " .setParams("+','.join(map(lambda t1:t1.type2.name+','+"'"+t1.name+"'",item.params))+")\\\n"+
            " .setResults("+','.join(map(lambda t1:t1.type2.name+','+"'"+t1.name+"'",item.results))+")")
            defstr.append('ns.addFunc(rfn)')
    return '\n'.join(defstr)


