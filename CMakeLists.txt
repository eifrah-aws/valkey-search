# --+ Auto generated file +--

cmake_minimum_required(VERSION 3.16)
project(VSS)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(submodules/abseil-cpp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR})

# Make absl include path available for all the sub projects
include_directories(${CMAKE_SOURCE_DIR}/submodules/abseil-cpp)

# protobuf helper method. Generated files are placed here: ${CMAKE_BINARY_DIR}/generated/${PROTO_OUT}, this means that
# PROTO_OUT must be relative path (or an empty string). The generated target will add to the include path
# ${CMAKE_BINARY_DIR}/generated
function(create_proto_library PROTO_FILE_SRC_DIR PROTO_OUT PROTO_BASE_FILE_NAME OUT_LIBNAME)
    find_package(Protobuf REQUIRED)
    set(PROTO_OUT ${CMAKE_BINARY_DIR}/generated/${PROTO_OUT})
    file(MAKE_DIRECTORY ${PROTO_OUT})
    execute_process(
        COMMAND ${Protobuf_PROTOC_EXECUTABLE} --cpp_out=${PROTO_OUT} --proto_path=${PROTO_FILE_SRC_DIR}
                ${PROTO_FILE_SRC_DIR}/${PROTO_BASE_FILE_NAME}.proto
        OUTPUT_VARIABLE PROTOC_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Creating target ${OUT_LIBNAME}")
    add_library(${OUT_LIBNAME} STATIC ${PROTO_OUT}/${PROTO_BASE_FILE_NAME}.pb.h
                                      ${PROTO_OUT}/${PROTO_BASE_FILE_NAME}.pb.cc)
    target_link_libraries(${OUT_LIBNAME} PUBLIC ${Protobuf_LIBRARIES})
    target_include_directories(${OUT_LIBNAME} PUBLIC ${CMAKE_BINARY_DIR}/generated)
    target_include_directories(${OUT_LIBNAME} PUBLIC ${Protobuf_INCLUDE_DIRS})
endfunction()

add_subdirectory(vmsdk)
add_subdirectory(third_party)
add_subdirectory(src)
