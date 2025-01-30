# --+ Auto generated file +--

cmake_minimum_required(VERSION 3.16)
project(VSS)

set(CMAKE_CXX_STANDARD 20 REQUIRED)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/submodules/highwayhash")
  message(WARNING "Could not locate submodule 'highwayhash'")
  message(FATAL_ERROR "Make sure to run 'git submodule update --init'")
endif()

if (UNIX AND NOT APPLE)
    function (pkg_config PKGNAME OUT_VARIABLE)
        if (NOT FOUND_PKGCONFIG)
            # Locate pkg-config once
            find_package(PkgConfig REQUIRED)
            set(FOUND_PKGCONFIG 1)
        endif ()
        pkg_check_modules(__PREFIX REQUIRED ${PKGNAME})
        message(STATUS "Found library for '${PKGNAME}': ${__PREFIX_LIBRARIES}")
        set(${OUT_VARIABLE}
            "${__PREFIX_LIBRARIES}"
            PARENT_SCOPE)
    endfunction ()
endif()

if (APPLE)
    find_package(grpc CONFIG REQUIRED)
    # brew install absl
    find_package(absl REQUIRED)
    # brew install googletest
    find_package(GTest REQUIRED)
    get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
    message(STATUS "gRPC C++ plugin: ${grpc_cpp_plugin_location}")
else()
    # sudo apt install libprotoc-dev libprotobuf-dev
    # git clone --recurse-submodules --shallow-submodules https://github.com/grpc/grpc
    # cmake -DgRPC_INSTALL=ON       -DgRPC_BUILD_TESTS=OFF       -DCMAKE_CXX_STANDARD=20       -DCMAKE_INSTALL_PREFIX=/usr/local ..
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}:/usr/local/lib/cmake/grpc:/usr/local/lib/cmake/absl:/usr/local/lib/protobuf")
    find_program(grpc_cpp_plugin_location grpc_cpp_plugin HINTS /usr/local/bin /usr/bin)
    find_program(Protobuf_PROTOC_EXECUTABLE protoc HINTS /usr/local/bin /usr/bin)

    if (NOT GRPC_LIB)
        pkg_config(grpc GRPC_LIB)
        set(GRPC_LIB ${GRPC_LIB} CACHE STRING "GRPC_LIB")
    endif ()
    message(STATUS "GRPC_LIB => ${GRPC_LIB}")
    message(STATUS "gRPC C++ plugin: ${grpc_cpp_plugin_location}")
endif()

if (UNIX AND NOT APPLE)
    set(protobuf_generate_PROTOC_EXE ${Protobuf_PROTOC_EXECUTABLE})
    message(STATUS "protobuf_generate_PROTOC_EXE is set to ${protobuf_generate_PROTOC_EXE}")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
    include(protobuf_generate)
endif ()

# target: highwayhash
add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/highwayhash)

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
