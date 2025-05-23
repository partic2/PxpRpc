
cmake_minimum_required(VERSION 3.15)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(pxprpc)

add_library(pxprpc STATIC pxprpc/pxprpc.c)

target_include_directories(pxprpc PUBLIC include)

add_library(pxprpc_pipe STATIC pxprpc_pipe/pxprpc_pipe.c)

target_include_directories(pxprpc_pipe PUBLIC include)

# To enable pxprpc_libuv target,
# add_subdirectory(libuv build-libuv EXCLUDE_FROM_ALL) before add this project
if(TARGET uv_a)
  add_library(pxprpc_libuv STATIC pxprpc_libuv/pxprpc_libuv.c)
  target_link_libraries(pxprpc_libuv pxprpc uv_a)
endif()

