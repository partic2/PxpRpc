

from typing import Optional,Any
import os
import io

from . import idldef


class LangToIDL(object):
    def toIDL(self,source:str)->Optional[idldef.Namespace]:
        raise NotImplemented()
    def filter(self,filename:str)->bool:
        raise NotImplemented()

class IDLToBinding(object):
    def toBinding(self,ns:idldef.Namespace)->str:
        raise NotImplemented()
    def fileName(self,ns:idldef.Namespace)->str:
        return ns.name
    def writeConfigFile(self,dir:str):
        raise NotImplemented

class GeneratorWorkspace(object):
    def __init__(self):
        self.sourceDirs=[]
        self.destination=None
        self.l2i=None
        self.i2b=None

    def addSourceDir(self,srcdir:str):
        self.sourceDirs.append(srcdir)
        return self
    
    def setDestination(self,destdir:str):
        self.destination=destdir
        return self

    def setProcessor(self,l2i:LangToIDL,i2b:IDLToBinding):
        self.l2i=l2i
        self.i2b=i2b
        return self

    def process(self):
        assert self.l2i!=None
        assert self.i2b!=None
        assert self.destination!=None
        for dir in self.sourceDirs:
            for children in os.listdir(dir):
                file=os.path.sep.join([dir,children])
                if self.l2i.filter(file):
                    with io.open(file,'r',encoding='utf-8') as fio:
                        source=fio.read()
                    ns=self.l2i.toIDL(source)
                    if ns!=None:
                        with io.open(os.path.sep.join([self.destination,self.i2b.fileName(ns)]),'w',encoding='utf-8') as fio:
                            fio.write(self.i2b.toBinding(ns))
        self.i2b.writeConfigFile(self.destination)
                
                        
