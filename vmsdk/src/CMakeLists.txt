## --+ Auto generated file +--

add_subdirectory(status)
add_subdirectory(valkey_module_api)
set(SRCS_LOG
    ${CMAKE_CURRENT_LIST_DIR}/log.cc
    ${CMAKE_CURRENT_LIST_DIR}/log.h
)

add_library(log STATIC ${SRCS_LOG})
target_include_directories(log PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(log PUBLIC managed_pointers)
target_link_libraries(log PUBLIC status_macros)
target_link_libraries(log PUBLIC valkey_module)

set(SRCS_UTILS
    ${CMAKE_CURRENT_LIST_DIR}/utils.cc
    ${CMAKE_CURRENT_LIST_DIR}/utils.h
)

add_library(utils STATIC ${SRCS_UTILS})
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(utils PUBLIC valkey_module)

set(SRCS_LATENCY_SAMPLER
    ${CMAKE_CURRENT_LIST_DIR}/latency_sampler.h
)

add_library(latency_sampler STATIC ${SRCS_LATENCY_SAMPLER})
target_include_directories(latency_sampler PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(latency_sampler PUBLIC utils)
target_link_libraries(latency_sampler PUBLIC hdrhistogram_c)

set(SRCS_MODULE_TYPE
    ${CMAKE_CURRENT_LIST_DIR}/module_type.cc
    ${CMAKE_CURRENT_LIST_DIR}/module_type.h
)

add_library(module_type STATIC ${SRCS_MODULE_TYPE})
target_include_directories(module_type PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(module_type PUBLIC log)
target_link_libraries(module_type PUBLIC managed_pointers)
target_link_libraries(module_type PUBLIC valkey_module)

set(SRCS_THREAD_POOL
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool.cc
    ${CMAKE_CURRENT_LIST_DIR}/thread_pool.h
)

add_library(thread_pool STATIC ${SRCS_THREAD_POOL})
target_include_directories(thread_pool PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set(SRCS_TIME_SLICED_MRMW_MUTEX
    ${CMAKE_CURRENT_LIST_DIR}/time_sliced_mrmw_mutex.cc
    ${CMAKE_CURRENT_LIST_DIR}/time_sliced_mrmw_mutex.h
)

add_library(time_sliced_mrmw_mutex STATIC ${SRCS_TIME_SLICED_MRMW_MUTEX})
target_include_directories(time_sliced_mrmw_mutex PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(time_sliced_mrmw_mutex PUBLIC utils)

set(SRCS_COMMAND_PARSER
    ${CMAKE_CURRENT_LIST_DIR}/command_parser.h
)

add_library(command_parser STATIC ${SRCS_COMMAND_PARSER})
target_include_directories(command_parser PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(command_parser PUBLIC managed_pointers)
target_link_libraries(command_parser PUBLIC type_conversions)
target_link_libraries(command_parser PUBLIC status_macros)
target_link_libraries(command_parser PUBLIC valkey_module)

set(SRCS_MODULE
    ${CMAKE_CURRENT_LIST_DIR}/module.cc
    ${CMAKE_CURRENT_LIST_DIR}/module.h
)

add_library(module STATIC ${SRCS_MODULE})
target_include_directories(module PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(module PUBLIC log)
target_link_libraries(module PUBLIC memory_allocation)
target_link_libraries(module PUBLIC utils)
target_link_libraries(module PUBLIC valkey_module)

set(SRCS_MEMORY_ALLOCATION_OVERRIDES
    ${CMAKE_CURRENT_LIST_DIR}/memory_allocation_overrides.cc
    ${CMAKE_CURRENT_LIST_DIR}/memory_allocation_overrides.h
)

add_library(memory_allocation_overrides STATIC ${SRCS_MEMORY_ALLOCATION_OVERRIDES})
target_include_directories(memory_allocation_overrides PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(memory_allocation_overrides PUBLIC memory_allocation)
target_link_libraries(memory_allocation_overrides PUBLIC valkey_module)

set(SRCS_MEMORY_ALLOCATION
    ${CMAKE_CURRENT_LIST_DIR}/memory_allocation.cc
    ${CMAKE_CURRENT_LIST_DIR}/memory_allocation.h
)

add_library(memory_allocation STATIC ${SRCS_MEMORY_ALLOCATION})
target_include_directories(memory_allocation PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set(SRCS_TYPE_CONVERSIONS
    ${CMAKE_CURRENT_LIST_DIR}/type_conversions.h
)

add_library(type_conversions INTERFACE ${SRCS_TYPE_CONVERSIONS})
target_include_directories(type_conversions INTERFACE ${CMAKE_CURRENT_LIST_DIR})

set(SRCS_MANAGED_POINTERS
    ${CMAKE_CURRENT_LIST_DIR}/managed_pointers.h
)

add_library(managed_pointers STATIC ${SRCS_MANAGED_POINTERS})
target_include_directories(managed_pointers PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(managed_pointers PUBLIC utils)
target_link_libraries(managed_pointers PUBLIC valkey_module)

