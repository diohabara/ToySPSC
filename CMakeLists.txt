cmake_minimum_required(VERSION 3.28)
project(ToySPSC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(TOYSPSC_ENABLE_COVERAGE "Build with gcov instrumentation" OFF)
option(TOYSPSC_ENABLE_BENCHMARKS "Build microbenchmarks" OFF)

add_compile_options(-Wall -Wextra -Wpedantic -fno-omit-frame-pointer)

if(TOYSPSC_ENABLE_COVERAGE)
  add_compile_options(--coverage -O0 -g)
  add_link_options(--coverage)
endif()

add_library(toyspsc_lib INTERFACE)
target_include_directories(toyspsc_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(toyspsc src/main.cc)
target_link_libraries(toyspsc PRIVATE toyspsc_lib)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)
add_subdirectory(test)

if(TOYSPSC_ENABLE_BENCHMARKS)
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
  )
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
  add_subdirectory(bench)
endif()
