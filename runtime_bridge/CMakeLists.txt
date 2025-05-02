
cmake_minimum_required(VERSION 3.19)


cmake_policy(SET CMP0003 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)


project(pxprpc_rtbridge)

option(PXPRPC_RTBRIDGE_BUILD_EXE "Indicate whether build standalone executable pxprpc_rtbridge host." OFF)

#Seemed shared object on linux require this options to build.
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()


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


macro(pxprpc_rtbridge_add_module entry source)
  list(APPEND PXPRPC_RTBRIDGE_MODULES_ENTRY ${entry})
  file(REAL_PATH ${source} abs_source BASE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
  list(APPEND PXPRPC_RTBRIDGE_MODULES_SOURCE ${abs_source})
endmacro()

macro(pxprpc_rtbridge_add_link_library)
  list(APPEND PXPRPC_RTBRIDGE_LINK_LIBRARY ${ARGV})
endmacro()

list(APPEND PXPRPC_RTBIRDGE_MODULES_DIRS "c-modules")

foreach(mod_dir ${PXPRPC_RTBIRDGE_MODULES_DIRS})
  file(GLOB module_list "${mod_dir}/*/def.cmake")
  foreach(module ${module_list})
    include(${module})
  endforeach(module)
endforeach()



file(WRITE "src/__gen-mod-entry.txt" "")
foreach(entry ${PXPRPC_RTBRIDGE_MODULES_ENTRY})
  file(APPEND "src/__gen-mod-entry.txt" "&${entry},\n")
endforeach(entry)

file(WRITE "src/__gen-mod-include.txt" "")
foreach(source ${PXPRPC_RTBRIDGE_MODULES_SOURCE})
  file(APPEND "src/__gen-mod-include.txt" "#include \"${source}\"\n")
endforeach(source)

list(APPEND pxprpc_rtbridge_sources src/pxprpc_rtbridge.c src/pxprpc_rtbridge_host.cpp)

list(APPEND PXPRPC_RTBRIDGE_LINK_LIBRARY pxprpc pxprpc_pipe pxprpc_libuv)

list(REMOVE_DUPLICATES PXPRPC_RTBRIDGE_LINK_LIBRARY)


add_library(pxprpc_rtbridge SHARED ${pxprpc_rtbridge_sources})
target_include_directories(pxprpc_rtbridge PUBLIC include)
target_link_libraries(pxprpc_rtbridge PUBLIC ${PXPRPC_RTBRIDGE_LINK_LIBRARY})
foreach(mod_dir ${PXPRPC_RTBIRDGE_MODULES_DIRS})
  target_include_directories(pxprpc_rtbridge PRIVATE ${mod_dir})
endforeach()

#PXPRPC_RTBRIDGE_BUILD_EXE build standalone executable 
if(PXPRPC_RTBRIDGE_BUILD_EXE)
  add_executable(pxprpc_rtbridge_exe ${pxprpc_rtbridge_sources})
  target_include_directories(pxprpc_rtbridge_exe PUBLIC include)
  target_link_libraries(pxprpc_rtbridge_exe PUBLIC ${PXPRPC_RTBRIDGE_LINK_LIBRARY})
  target_compile_definitions(pxprpc_rtbridge_exe PRIVATE PXPRPC_RTBRIDGE_BUILD_EXE)
  foreach(mod_dir ${PXPRPC_RTBIRDGE_MODULES_DIRS})
    target_include_directories(pxprpc_rtbridge_exe PRIVATE ${mod_dir})
  endforeach()
endif(PXPRPC_RTBRIDGE_BUILD_EXE)


if(PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER)
  target_compile_definitions(pxprpc_rtbridge PRIVATE PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER)
  if(PXPRPC_RTBRIDGE_BUILD_EXE)
    target_compile_definitions(pxprpc_rtbridge_exe PRIVATE PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER)
  endif(PXPRPC_RTBRIDGE_BUILD_EXE)
endif(PXPRPC_RTBRIDGE_ENABLE_TEST_TCPSERVER)




