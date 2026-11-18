cmake_minimum_required(VERSION 3.16)
project(OrderFlow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(ORDERFLOW_BUILD_TESTS "Build unit tests (downloads GoogleTest)" ON)
option(ORDERFLOW_SANITIZE "Build with ThreadSanitizer to catch data races" OFF)

find_package(Threads REQUIRED)

if(ORDERFLOW_SANITIZE)
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()

add_library(orderflow_core
  src/inventory.cpp
  src/simulator.cpp
)
target_include_directories(orderflow_core PUBLIC include)
target_link_libraries(orderflow_core PUBLIC Threads::Threads)
target_compile_options(orderflow_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(orderflow src/main.cpp)
target_link_libraries(orderflow PRIVATE orderflow_core)
target_compile_options(orderflow PRIVATE -Wall -Wextra -Wpedantic)

if(ORDERFLOW_BUILD_TESTS)
  # Use an installed GoogleTest (e.g. apt install libgtest-dev) if present,
  # otherwise fetch it from GitHub.
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
  endif()

  enable_testing()
  add_executable(orderflow_tests tests/test_orderflow.cpp)
  target_link_libraries(orderflow_tests PRIVATE orderflow_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(orderflow_tests)
endif()
