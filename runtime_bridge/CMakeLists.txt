
cmake_minimum_required(VERSION 3.15)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(pxprpc_rtbridge)



list(APPEND pxprpc_rtbridge_sources src/pxprpc_rtbridge.c src/pxprpc_rtbridge_host.cpp)

# Check jni.h directly first.
include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX("jni.h" JNI_H_FOUND)

if(NOT JNI_H_FOUND)
  #fallback to FindJNI (use JAVA_HOME)
  find_package(JNI)
  if(JNI_FOUND)
    include_directories(${JNI_INCLUDE_DIRS})
    set(JNI_H_FOUND 1)
  endif()
endif()

if(JNI_H_FOUND)
message("java jnibridge enabled")
add_compile_definitions(PXPRPC_RTBRIDGE_HAVE_JNI_H=1)
endif()

if(NOT TARGET pxprpc)
add_subdirectory("../c" build-pxprpc)
endif()


add_library(pxprpc_rtbridge SHARED ${pxprpc_rtbridge_sources})
target_include_directories(pxprpc_rtbridge PUBLIC include)
target_link_libraries(pxprpc_rtbridge PRIVATE pxprpc pxprpc_pipe pxprpc_libuv)


add_executable(pxprpc_rtbridge_host_test ${pxprpc_rtbridge_sources})
target_include_directories(pxprpc_rtbridge_host_test PUBLIC include)
target_link_libraries(pxprpc_rtbridge_host_test PRIVATE pxprpc pxprpc_pipe pxprpc_libuv)
target_compile_definitions(pxprpc_rtbridge_host_test PRIVATE PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER_EXE)




