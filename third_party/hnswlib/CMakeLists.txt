# --+ Auto generated file +--

set(SRCS_HNSWLIB_VMSDK
    ${CMAKE_CURRENT_LIST_DIR}/bruteforce.h
    ${CMAKE_CURRENT_LIST_DIR}/hnswalg.h
    ${CMAKE_CURRENT_LIST_DIR}/hnswlib.h
    ${CMAKE_CURRENT_LIST_DIR}/space_ip.h
    ${CMAKE_CURRENT_LIST_DIR}/space_l2.h
    ${CMAKE_CURRENT_LIST_DIR}/stop_condition.h
    ${CMAKE_CURRENT_LIST_DIR}/visited_list_pool.h)

add_library(hnswlib_vmsdk STATIC ${SRCS_HNSWLIB_VMSDK})
target_include_directories(hnswlib_vmsdk PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(hnswlib_vmsdk PUBLIC iostream)
target_link_libraries(hnswlib_vmsdk PUBLIC simsimd)
target_link_libraries(hnswlib_vmsdk PUBLIC memory_allocation_overrides)
target_link_libraries(hnswlib_vmsdk PUBLIC status_macros)

set(SRCS_SIMSIMD ${CMAKE_CURRENT_LIST_DIR}/simsimd.h)

add_library(simsimd INTERFACE ${SRCS_SIMSIMD})
target_include_directories(simsimd INTERFACE ${CMAKE_CURRENT_LIST_DIR})

set(SRCS_IOSTREAM ${CMAKE_CURRENT_LIST_DIR}/iostream.h)

add_library(iostream INTERFACE ${SRCS_IOSTREAM})
target_include_directories(iostream INTERFACE ${CMAKE_CURRENT_LIST_DIR})
