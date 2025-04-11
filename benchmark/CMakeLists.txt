add_executable(
    ${PROJECT_NAME}_benchmark
        main.cpp
        cm_sketch_benchmark.cpp
        tiny_lfu_cms_benchmark.cpp
        large_page_benchmark.cpp
        bloom_filter_benchmark.cpp
        small_page_find_benchmark.cpp
)

target_include_directories(
    ${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_SOURCE_DIR}/core/include
)
target_link_libraries(
    ${PROJECT_NAME}_benchmark benchmark::benchmark
)
