cmake_minimum_required(VERSION 3.15)
project(
    cache
    LANGUAGES CXX
)

include(FetchContent)

include(cmake/CompileOptions.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Environment.cmake)

add_subdirectory(core)

add_executable(
    ${PROJECT_NAME} main.cpp
)
target_link_libraries(
    ${PROJECT_NAME} ${PROJECT_NAME}_objs
)

include(CTest)
add_subdirectory(test)

add_subdirectory(third_party/benchmark)
add_subdirectory(benchmark)
