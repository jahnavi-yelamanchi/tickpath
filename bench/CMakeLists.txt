# Toolchain check: proves Google Benchmark and HdrHistogram build and link
# under the project flags. Replaced by the replay harness in a later issue.
add_executable(bench_toolchain_check toolchain_check.cpp)
target_link_libraries(bench_toolchain_check PRIVATE tickpath::core benchmark::benchmark hdr_histogram_static)
tickpath_target_defaults(bench_toolchain_check)
