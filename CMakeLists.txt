# --+ Auto generated file +--

cmake_minimum_required(VERSION 3.16)
project(VSS)

set(CMAKE_CXX_STANDARD 20 REQUIRED)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/submodules/abseil-cpp")
  message(WARNING "Could not locate submodule 'abseil'")
  message(FATAL_ERROR "Make sure to run 'git submodule update --init'")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/submodules/googletest")
  message(WARNING "Could not locate submodule 'googletest'")
  message(FATAL_ERROR "Make sure to run 'git submodule update --init'")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/submodules/highwayhash")
  message(WARNING "Could not locate submodule 'highwayhash'")
  message(FATAL_ERROR "Make sure to run 'git submodule update --init'")
endif()

find_package(grpc CONFIG REQUIRED)
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
message(STATUS "gRPC C++ plugin: ${grpc_cpp_plugin_location}")

add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/abseil-cpp)
add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/googletest)
# target: highwayhash
add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/highwayhash)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR})

# Make absl / gtest include path available for all the sub projects
include_directories(${CMAKE_SOURCE_DIR}/submodules/abseil-cpp)
include_directories(
  ${CMAKE_SOURCE_DIR}/submodules/googletest/googletest/include)

# Helper method: create static library from a single proto file.
# "PROTO_FILE_SRC_DIR" contains the folder relative path to the proto file from
# top level source directory. "PROTO_BASE_FILE_NAME" contains the file name (no
# extension). "OUT_LIBNAME" is the user provided target name to create
function(create_proto_library PROTO_FILE_SRC_DIR PROTO_BASE_FILE_NAME
         OUT_LIBNAME)
  find_package(Protobuf REQUIRED)
  set(PROTO_OUT_DIR ${CMAKE_BINARY_DIR}/${PROTO_FILE_SRC_DIR})
  set(PROTO_IN_DIR ${CMAKE_SOURCE_DIR}/${PROTO_FILE_SRC_DIR})
  file(MAKE_DIRECTORY ${PROTO_OUT_DIR})
  if(NOT EXISTS ${PROTO_OUT}/${PROTO_BASE_FILE_NAME}.pb.h
     OR NOT EXISTS ${PROTO_OUT_DIR}/${PROTO_BASE_FILE_NAME}.pb.cc)
    execute_process(
      COMMAND
        ${Protobuf_PROTOC_EXECUTABLE} --cpp_out=${PROTO_OUT_DIR}
        --proto_path=${PROTO_IN_DIR}
        ${PROTO_IN_DIR}/${PROTO_BASE_FILE_NAME}.proto
      OUTPUT_VARIABLE PROTOC_OUTPUT
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  message(STATUS "Creating target ${OUT_LIBNAME}")
  set(__LIB_SRCS "${PROTO_OUT_DIR}/${PROTO_BASE_FILE_NAME}.pb.h"
                 "${PROTO_OUT_DIR}/${PROTO_BASE_FILE_NAME}.pb.cc")
  add_library(${OUT_LIBNAME} STATIC ${__LIB_SRCS})
  target_link_libraries(${OUT_LIBNAME} PUBLIC ${Protobuf_LIBRARIES})
  target_include_directories(${OUT_LIBNAME} PUBLIC ${CMAKE_BINARY_DIR})
  target_include_directories(${OUT_LIBNAME} PUBLIC ${PROTO_OUT_DIR})
  target_include_directories(${OUT_LIBNAME} PUBLIC ${Protobuf_INCLUDE_DIRS})
  unset(PROTO_OUT_DIR CACHE)
  unset(PROTO_IN_DIR CACHE)
  unset(__LIB_SRCS CACHE)
endfunction()

add_subdirectory(vmsdk)
add_subdirectory(third_party)
add_subdirectory(src)
