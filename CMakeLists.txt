cmake_minimum_required(VERSION 3.25)

project(tickpath
  VERSION 0.1.0
  DESCRIPTION "Nasdaq TotalView-ITCH 5.0 feed handler, order book and pre-trade risk"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TICKPATH_BUILD_TESTS "Build tests" ON)
option(TICKPATH_BUILD_BENCH "Build benchmarks" ON)
option(TICKPATH_BUILD_FUZZ "Build libFuzzer targets (clang only)" OFF)
option(TICKPATH_CLANG_TIDY "Run clang-tidy on project targets" OFF)
set(TICKPATH_SANITIZE "" CACHE STRING "Sanitizers: empty, address,undefined or thread")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(Dependencies)

add_subdirectory(src)
add_subdirectory(tools)

if(TICKPATH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(TICKPATH_BUILD_BENCH)
  add_subdirectory(bench)
endif()

if(TICKPATH_BUILD_FUZZ)
  add_subdirectory(fuzz)
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE TICKPATH_FORMAT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${TICKPATH_FORMAT_SOURCES}
    COMMENT "clang-format (in place)")
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${TICKPATH_FORMAT_SOURCES}
    COMMENT "clang-format (check)")
endif()
