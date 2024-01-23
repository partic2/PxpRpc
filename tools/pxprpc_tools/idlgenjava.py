
from . import idldef
import javalang
import javalang.tree

import os
import io

from typing import Dict,cast,Any,Optional

from .idlcomm import LangToIDL

import logging
log1=logging.getLogger(__name__)


def convertJavaTypeToIdlType(jt:str):
    map={
        'void':None,
        'ByteBuffer':idldef.Bytes,
        'int':idldef.Int32,
        'long':idldef.Int64,
        'float':idldef.Float32,
        'double':idldef.Float64,
        'String':idldef.String,
        'boolean':idldef.Bool
    }
    if jt in map:
        return map[jt]
    else:
        return idldef.RemoteObject

def processJavaFuncInfo(funcinfo:Dict)->idldef.Function:
    idlfn=idldef.Function(funcinfo['name'])
    params=[]
    results=[]
    
    if funcinfo['typedecl']!='':
        tParam,tResult=funcinfo['typedecl'].split('->')
        for t1,t2 in enumerate(idldef.TypesFromTypedeclString(tParam)):
            params.append(idldef.TypedVar(t2,'arg'+str(t1)))
        for t1,t2 in enumerate(idldef.TypesFromTypedeclString(tResult)):
            results.append(idldef.TypedVar(t2,'ret'+str(t1)))

    startAt=0
    p1=funcinfo['parameters']
    if len(p1)>=1 and p1[0]['type']=='AsyncReturn':
        startAt=1
    if funcinfo['typedecl']=='':
        for t1 in range(startAt,len(p1)):
            params.append(idldef.TypedVar(convertJavaTypeToIdlType(p1[t1]['type']),p1[t1]['name']))
    else:
        for t1 in range(startAt,len(p1)):
            params[t1].name=p1[t1]['name']
    if funcinfo['typedecl']=='':
        r1=funcinfo['results']
        if len(r1)>0:
            r1=r1[0]
            results.append(idldef.TypedVar(convertJavaTypeToIdlType(r1['type']),r1['name']))
    idlfn.params=params
    idlfn.results=results
    return idlfn
            

def parseJavaSource(source:str):
    if not 'PxprpcNamespace' in source:
        return None
    t1=javalang.parse.parse(source)
    PxprpcNamespace='<noname>'
    funclist=[]
    for t2 in t1.types[0].body:
        if type(t2)==javalang.tree.MethodDeclaration:
            funcinfo=dict(parameters=[],results=[],typedecl='',name=t2.name)
            if t2.annotations:
                for t3 in t2.annotations:
                    if t3.name=='MethodTypeDecl':
                        funcinfo['typedecl']=t3.element.value[1:-1]
            for t3 in t2.parameters:
                funcinfo['parameters'].append(dict(name=t3.name,type=t3.type.name))
            if t2.return_type!=None:
                funcinfo['results'].append(dict(name='result',type=t2.return_type.name))
            funclist.append(funcinfo)
        elif type(t2)==javalang.tree.FieldDeclaration:
            if len(t2.declarators)==1:
                if t2.declarators[0].name=='PxprpcNamespace':
                    PxprpcNamespace=t2.declarators[0].initializer.value[1:-1]
    ns=idldef.Namespace(PxprpcNamespace)
    for t1 in funclist:
        ns.addFunc(processJavaFuncInfo(t1))
    return ns

class JavaLangToIDL(LangToIDL):

    def toIDL(self,source:str)->Optional[idldef.Namespace]:
        return parseJavaSource(source)

    def filter(self, filename: str) -> bool:
        return filename.endswith('.java')
