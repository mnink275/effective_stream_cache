add_executable(
    ${PROJECT_NAME}_benchmark
        main.cpp
        cm_sketch_benchmark.cpp
        tiny_lfu_cms_benchmark.cpp
)

target_link_libraries(
    ${PROJECT_NAME}_benchmark benchmark::benchmark
)
