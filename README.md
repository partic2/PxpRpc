# Pxp RPC 

### Introduce

PxpRpc(PARTIC cross platform remote procedure call) is a very tiny rpc library aim to call and interchange data cross platform and language with high performance and little load.


### Platform

Currently, pxprpc has been implemented on below platform

rpc server on Java (>=1.6)

asynchronous rpc server on C

rpc server over tcp on C with libuv

rpc server over tcp on C with tbox

rpc server and client on Python(>=3.8)

rpc server and client on Typescript(websocket)

==================================

rpc server on C# (.net) now is still on 1.0 version. May we can upgrade it to 2.0 in future.


### Detail help
See /documents/* for more information.

See test files for detail usage.

[C(libuv) example](c/pxprpc_libuv/test.cpp)

[C(tbox) example](c/pxprpc_tbox/test.cpp)

[Java example](java/src/pxprpc/test/PxpRpc.java)

[Python example](python/pxprpc/tests.py)

[Typescript(websocket) example](typescript/pxprpc/tests.ts)

[C#(.net) example](csharp/dotnet/pxprpc/tests/TestMain.cs)


Feel free to PR and issue

