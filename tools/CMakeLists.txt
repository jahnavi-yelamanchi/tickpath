add_executable(tickpath-gen gen_feed.cpp)
target_link_libraries(tickpath-gen PRIVATE tickpath::core)
tickpath_target_defaults(tickpath-gen TIDY)

add_executable(tickpath-replay replay.cpp)
target_link_libraries(tickpath-replay PRIVATE tickpath::core)
tickpath_target_defaults(tickpath-replay TIDY)
