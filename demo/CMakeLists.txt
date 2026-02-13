cmake_minimum_required(VERSION 3.16)

# Demo executables
add_executable(demo_game_simulation demo_game_simulation.c)
target_link_libraries(demo_game_simulation PRIVATE lgx_runtime pthread m)
target_include_directories(demo_game_simulation PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Install demo
install(TARGETS demo_game_simulation
        RUNTIME DESTINATION bin/lgx-demos)

# Install demo documentation
install(FILES README.md
        DESTINATION share/doc/lgx-runtime/demos)
