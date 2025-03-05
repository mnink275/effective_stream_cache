add_executable(
    ${PROJECT_NAME}_benchmark
        cm_sketch_benchmark.cpp
)

target_link_libraries(
    ${PROJECT_NAME}_benchmark benchmark::benchmark
)
