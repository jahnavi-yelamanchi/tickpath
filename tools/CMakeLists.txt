add_executable(tickpath-gen gen_feed.cpp)
target_link_libraries(tickpath-gen PRIVATE tickpath::core)
tickpath_target_defaults(tickpath-gen TIDY)
