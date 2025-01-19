FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/f8d7d77c06936315286eb55f8de22cd23c188571.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(
    ${PROJECT_NAME}_test tiny_lfu_test.cpp bloom_filter_test.cpp
)

target_link_libraries(
    ${PROJECT_NAME}_test GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)
