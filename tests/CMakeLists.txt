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
tickpath_add_test(test_generator LABELS infra SOURCES unit/test_generator.cpp)
tickpath_add_test(test_reference_book LABELS infra SOURCES book/test_reference_book.cpp)


# Infrastructure: must pass.
tickpath_add_test(test_differential_harness LABELS infra SOURCES differential/test_differential_harness.cpp)
tickpath_add_test(test_oracle_properties LABELS infra SOURCES property/test_oracle_properties.cpp)

# Owner-written hot path: fail until implemented.
tickpath_add_test(test_parser LABELS owner SOURCES itch/test_parser.cpp)
tickpath_add_test(test_fast_books LABELS owner SOURCES book/test_fast_books.cpp)
tickpath_add_test(test_differential_fast_books LABELS owner SOURCES differential/test_differential_fast_books.cpp)
tickpath_add_test(test_book_properties LABELS owner SOURCES property/test_book_properties.cpp)
tickpath_add_test(test_risk LABELS owner SOURCES risk/test_risk.cpp)
