include(GoogleTest)

# tickpath_add_test(<name> LABELS <label>... SOURCES <src>...)
# Labels: "infra" for tests of code in this repo's support layer (must pass),
# "owner" for tests of the owner-written hot path (fail until implemented).
function(tickpath_add_test name)
  cmake_parse_arguments(ARG "" "" "LABELS;SOURCES;LIBS" ${ARGN})
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE tickpath::core GTest::gtest_main rapidcheck rapidcheck_gtest ${ARG_LIBS})
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  tickpath_target_defaults(${name})
  gtest_discover_tests(${name}
    PROPERTIES LABELS "${ARG_LABELS}"
    DISCOVERY_TIMEOUT 60)
endfunction()

tickpath_add_test(test_build LABELS infra SOURCES unit/test_build.cpp)
tickpath_add_test(test_messages LABELS infra SOURCES unit/test_messages.cpp)
