# All third-party code comes in through FetchContent, pinned to a tag or
# commit. Versions are recorded in docs/VERSIONS.md.

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

if(TICKPATH_BUILD_TESTS)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
    SYSTEM)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  # RapidCheck publishes no release tags; pinned to a master commit.
  FetchContent_Declare(rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        2c3c4365aca21ef4e612768fdc95b1ce8b39a651
    SYSTEM)
  set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
  set(RC_INSTALL_ALL_EXTRAS OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest rapidcheck)
endif()

if(TICKPATH_BUILD_BENCH)
  FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.4
    GIT_SHALLOW    TRUE
    SYSTEM)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(hdr_histogram
    GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
    GIT_TAG        0.11.8
    GIT_SHALLOW    TRUE
    SYSTEM)
  set(HDR_HISTOGRAM_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(HDR_HISTOGRAM_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(HDR_HISTOGRAM_INSTALL_SHARED OFF CACHE BOOL "" FORCE)
  set(HDR_HISTOGRAM_INSTALL_STATIC OFF CACHE BOOL "" FORCE)
  set(HDR_LOG_REQUIRED "DISABLED" CACHE STRING "" FORCE)

  FetchContent_MakeAvailable(benchmark hdr_histogram)
endif()
