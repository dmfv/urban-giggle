cmake_minimum_required(VERSION 3.1...3.29)

project(Grep VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_library(GrepLib grep.cpp)

add_executable(MyGrep main.cpp grep.cpp)
target_link_libraries(MyGrep GrepLib)

add_executable(GrepTest tests/tests.cpp)

target_link_libraries(
    GrepTest
    GrepLib
    GTest::gtest_main)
    
include(GoogleTest)
gtest_discover_tests(GrepTest)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_test(
  NAME GrepTest
  COMMAND $<TARGET_FILE:GrepTest>
)

target_compile_definitions(GrepTest PRIVATE TEST_DIR_MACRO="${CMAKE_SOURCE_DIR}/tests")
