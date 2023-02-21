
from . import idldef
import typing


import logging
log1=logging.getLogger(__name__)

def convertJavaTypeToIdlType(jt:str):
    map={
        'void':None,
        'String':idldef.String,
        'byte[]':idldef.Bytes,
        'ByteBuffer':idldef.Bytes,
        'int':idldef.Int32,
        'long':idldef.Int64,
        'float':idldef.Float32,
        'double':idldef.Float64
    }
    if jt in map:
        return map[jt]
    else:
        return idldef.RemoteObject

def parseNamespaceDeclareFromClass(classDecl:str):
    import re
    declLvl=0
    validDecl=''
    funcs=[]
    name=''
    foundname=re.findall('public static final String PxprpcNamespace="(.+)";',classDecl)
    if len(foundname)>0:
        name=foundname[0]
    
    for ch in classDecl:
        if ch=='{':
            declLvl+=1
        elif ch=='}':
            declLvl-=1
        if declLvl==1:
            validDecl+=ch
    for funcDecl in re.findall('public .+\\(.*\\)',validDecl):
        if '=' in funcDecl:
            # variable initialize
            continue
        t1=parseFunctionDeclare(funcDecl)
        if t1!=None:
            funcs.append(t1)
    return idldef.Namespace(name).addFunc(funcs)


def parseFunctionDeclare(funcDecl:str):
    import re
    log1.debug('parse function:'+funcDecl)
    # remove useless symbol
    funcDecl=re.sub('<.*>','',funcDecl)
    funcDecl=funcDecl.replace('(',' ( ').replace(')',' ) ').replace(',',' ')
    toks=re.split('\\s+',funcDecl)
    result:typing.Optional[idldef.NamedValue]=None

    t1=0
    while t1<len(toks) and toks[t1] in ['public','static','final']:
        t1+=1

    result=idldef.NamedValue('result',convertJavaTypeToIdlType(toks[t1]))
    t1+=1
    func=idldef.Function(toks[t1])
    if toks[t1]=='(':
        # possible constructor, skip
        return None
    t1+=2  # skip '('
    limit=len(toks)
    while t1<limit:
        if toks[t1]==')':
            break
        elif toks[t1] in ['final']:
            t1+=1
            continue
        elif toks[t1].startswith('AsyncReturn'):
            t1+=2
            result.type2=idldef.RemoteObject
            continue
        t2=idldef.NamedValue(toks[t1+1],convertJavaTypeToIdlType(toks[t1]))
        func.params.append(t2)
        t1+=2
    
    if result!=None and result.type2!=None:
        func.result=result
    return func
