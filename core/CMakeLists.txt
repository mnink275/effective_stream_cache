set(INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(HEADERS
    ${INCLUDE_PATH}/bloom_filter_simple.hpp
    ${INCLUDE_PATH}/bloom_filter.hpp
    ${INCLUDE_PATH}/cache.hpp
    ${INCLUDE_PATH}/cm_sketch.hpp
    ${INCLUDE_PATH}/lru.hpp
    ${INCLUDE_PATH}/tiny_lfu_cms.hpp
    ${INCLUDE_PATH}/utils.hpp
)

set(SOURCES

)

add_library(
    ${PROJECT_NAME}_objs OBJECT ${HEADERS}
)
target_include_directories(
    ${PROJECT_NAME}_objs PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
)
