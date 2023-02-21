import typing

        
class Type2:
    def __init__(self,name:str):
        self.name=name

class NamedValue:
    def __init__(self,name:str,type2:Type2):
        self.name=name
        self.type2=type2
        
        
class Function:
    def __init__(self,name:str,params:typing.Optional[typing.List[NamedValue]]=None,result:typing.Optional[NamedValue]=None):
        self.name=name
        self.params:typing.List[NamedValue]=[] if params == None else params
        self.result=result

    def setParams(self,*typeValuePair)->'Function':
        for t1 in range(0,len(typeValuePair),2):
            self.params.append(NamedValue(typeValuePair[t1+1],typeValuePair[t1]))
        return self

    def setResult(self,t:Type2,name='result')->'Function':
        self.result=NamedValue(name,t)
        return self
        
class Namespace:
    def __init__(self,name:str):
        self.name=name
        self.content=[]

    def addType(self,t:Type2):
        self.content.append(t)
        return self

    def addFunc(self,fn:typing.Union[Function,typing.Iterable[Function]]):
        if isinstance(fn,Function):
            self.content.append(fn)
        else:
            self.content+=fn
        return self

        

Bool=Type2('Bool')
Int32=Type2('Int32')
Int64=Type2('Int64')
Float32=Type2('Float32')
Float64=Type2('Float64')
RemoteBytes=Type2('RemoteBytes')
RemoteObject=Type2('RemoteObject')

Bytes=Type2('Bytes')
String=Type2('String')


class DataObjectType(Type2):
    def __init__(self,name:str):
        self.name=name
        self.props:typing.List[NamedValue]=[]
        
    def prop(self,name:str,type2:Type2)->'DataObjectType':
        self.props.append(NamedValue(name,type2))
        return self
        
    
class DataObjectArrayType(Type2):
    def __init__(self,name:str):
        self.name=name
        self.props:typing.List[NamedValue]=[]
    
    def prop(self,name:str,type2:Type2)->'DataObjectArrayType':
        self.props.append(NamedValue(name,type2))
        return self
        
        