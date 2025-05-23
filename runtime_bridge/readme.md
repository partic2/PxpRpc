
# Runtime bridge

runtime_bridge is a library aim to make a standard alone pxprpc server to expose native API as RPC interface.
Also, support pxprpc connection in ONE process connecting differenct runtime/language.


## Write/Apply your c/cpp modules

runtime_bridge_host use header only code structure, Please refer [c-modules](c-modules/readme.md) and the ["base" module](c-modules/base) to write/apply your own module.

## Build

You may need run `python deps/pull_deps.py` first to pull the dependecies.