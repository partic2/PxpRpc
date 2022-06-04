# Pxp RPC 

### Introduce

PxpRpc(PARTIC cross platform remote procedure call) is a very tiny rpc library aim to call and interchange data cross platform and language with high performance and little load.


### Platform

Currently, pxprpc has been implemented on below platform

rpc server on java (>=1.6)

asynchronous rpc server on c

rpc server over tcp on c with libuv

rpc server over tcp on c with tbox

rpc server and client on python(>=3.8)

rpc client on typescript(websocket)


### Detail help
See /documents/* for more information.

See test files for detail usage.

[C(libuv) example](c/pxprpc_libuv/test.cpp)

[C(tbox) example](c/pxprpc_tbox/test.cpp)

[Java example](java/src/pursuer/test/PxpRpc.java)

[Python example](python/pxprpc/tests.py)

[Typescript(websocket) example](typescript/pxprpc/tests.ts)


Feel free to PR and issue

