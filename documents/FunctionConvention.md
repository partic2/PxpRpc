

### common function convetion

pxprpc high level abstract treat all callable as a model like below.

(input arguments)->call->(output arguments)

function signature
format: 'parameters type->return type' 
eg:
a function defined in c:
bool fn(uin32_t,uint64_t,float64_t,struct pxprpc_object *)
defined in java:
boolean fn(int,int,double,Object)
...
it's pxprpc signature: 
iido->c

available type signature characters:
i  int(32bit integer)
l  long(64bit integer) 
f  float(32bit float)
d  double(64bit float)
o  object(32bit reference address)
b  bytes(bytes buffer)
'' return void(32bit 0)

c  boolean(pxprpc use 1byte(1/0) to store a boolean value)
s  string(bytes will be decode to string)

