

## Use pxprpc with Makefile

Set environment variable "PXPRPC_SOURCE_DIR" to the path of this project.

In makefile, add make config script by "include", like below

`include $(PXPRPC_SOURCE_DIR)/c/pxprpc_libuv/make-config.mk`

depend on which backend you choose.

or just

`include $(PXPRPC_SOURCE_DIR)/c/pxprpc/make-config.mk`

for custom backend.




These scripts will add build flags into variable like `$(PXPRPC_LDFLAGS)`, `$(PXPRPC_LIBUV_CFLAGS)` ,etc.

then build like below

```
your_target:build_pxprpc_libuv
	$(CC) -o yoursource.cpp $(CFLAGS) $(PXPRPC_LIBUV_CFLAGS) test.cpp $(LDFLAGS) $(PXPRPC_LIBUV_LDFLAGS) -lstdc++
```

See also pxprpc_libuv/make-config.mk, which contains a build example.


You may need set `$(LIBUV_CFLAGS)` and `$(LIBUV_LDFLAGS)` variable if use pxprpc_libuv backend. 

