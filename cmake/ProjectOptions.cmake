# Per-target settings. Applied only to project targets so third-party code
# fetched with FetchContent is not built with -Werror or clang-tidy.

set(TICKPATH_WARNINGS
  -Wall -Wextra -Wpedantic -Werror
  -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  -Wnon-virtual-dtor -Woverloaded-virtual
  -Wdouble-promotion -Wfloat-equal -Wimplicit-fallthrough)
# GCC's -Wnull-dereference fires inside libstdc++ <variant> at -O3 (false
# positive); keep it for clang only.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND TICKPATH_WARNINGS -Wnull-dereference)
endif()

set(TICKPATH_SANITIZER_FLAGS "")
if(TICKPATH_SANITIZE)
  set(TICKPATH_SANITIZER_FLAGS -fsanitize=${TICKPATH_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
  # Sanitizers apply to every target, including dependencies, so the
  # instrumented and uninstrumented code never mix (required for tsan).
  add_compile_options(${TICKPATH_SANITIZER_FLAGS})
  add_link_options(${TICKPATH_SANITIZER_FLAGS})
endif()

if(TICKPATH_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy-18 clang-tidy REQUIRED)
endif()

# tickpath_target_defaults(<target> [TIDY])
function(tickpath_target_defaults target)
  cmake_parse_arguments(ARG "TIDY" "" "" ${ARGN})
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "INTERFACE_LIBRARY")
    return()
  endif()
  target_compile_options(${target} PRIVATE ${TICKPATH_WARNINGS})
  if(ARG_TIDY AND TICKPATH_CLANG_TIDY)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endfunction()
