# --+ Auto generated file +--

set(SRCS_HDRHISTOGRAM_C
    ${CMAKE_CURRENT_LIST_DIR}/hdr_encoding.c
    ${CMAKE_CURRENT_LIST_DIR}/hdr_histogram.c
    ${CMAKE_CURRENT_LIST_DIR}/hdr_interval_recorder.c
    ${CMAKE_CURRENT_LIST_DIR}/hdr_thread.c
    ${CMAKE_CURRENT_LIST_DIR}/hdr_time.c
    ${CMAKE_CURRENT_LIST_DIR}/hdr_writer_reader_phaser.c
    ${CMAKE_CURRENT_LIST_DIR}/hdr_atomic.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_encoding.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_endian.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_histogram.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_interval_recorder.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_tests.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_thread.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_time.h
    ${CMAKE_CURRENT_LIST_DIR}/hdr_writer_reader_phaser.h)

add_library(hdrhistogram_c STATIC ${SRCS_HDRHISTOGRAM_C})
target_include_directories(hdrhistogram_c PUBLIC ${CMAKE_CURRENT_LIST_DIR})
