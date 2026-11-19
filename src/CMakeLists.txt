configure_file(build_config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/tickpath/detail/build_config.hpp @ONLY)

add_library(tickpath_core STATIC
  build_info.cpp
  itch/encode.cpp
  sim/generator.cpp
  sim/fault_injector.cpp
  book/reference_book.cpp
  stubs.cpp)
target_include_directories(tickpath_core PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_features(tickpath_core PUBLIC cxx_std_20)
tickpath_target_defaults(tickpath_core TIDY)
add_library(tickpath::core ALIAS tickpath_core)
