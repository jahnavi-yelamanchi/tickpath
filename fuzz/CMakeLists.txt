if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "TICKPATH_BUILD_FUZZ requires clang (libFuzzer)")
endif()

# tickpath_add_fuzzer(<name> <source>)
function(tickpath_add_fuzzer name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE tickpath::core)
  target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
  target_link_options(${name} PRIVATE -fsanitize=fuzzer)
  tickpath_target_defaults(${name})
endfunction()

tickpath_add_fuzzer(fuzz_parser fuzz_parser.cpp)
