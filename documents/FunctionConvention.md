

### common function convetion

pxprpc high level abstract treat all callable as a model like below.

(input arguments)->call->(output arguments)

function typedecl
format: 'parameters type->returns type' 
eg:
a function defined like:
[bool,int] fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
it's pxprpc typedecl: 
iido->ci

available type typedecl characters:
i  int(32bit integer)
l  long(64bit integer) 
f  float(32bit float)
d  double(64bit float)
o  object(32bit reference address)
b  bytes(bytes buffer)
'' return void(32bit 0)

c  boolean(pxprpc use 1byte(1/0) to store a boolean value)
s  string(bytes will be decode to string)

