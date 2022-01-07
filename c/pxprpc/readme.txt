

If you want to use "pxprpc for c" with socket, pipe, etc. consider using pxprpc_tbox, or pxprpc_libuv(WIP).

You can alse implement the struct "pxprpc_abstract_io" yourself as underlying stream. 

The API should be used like below:


    struct pxprpc_server_api *servapi;

    pxprpc_server_query_interface(&servapi);
    servapi->context_new(&sockconn->rpcCtx,&sockconn->io1,sCtx->namedFunc,sCtx->lenOfNamedFunc);
    ... ...


there is c++ header-only wrap for c api in "cppwrap".
