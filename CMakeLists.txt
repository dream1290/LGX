cmake_minimum_required(VERSION 3.20)
project(lgx_runtime VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Runtime library
add_library(lgx_runtime SHARED
    src/lgx_runtime.c
)

target_include_directories(lgx_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Test game
add_executable(test_game
    tests/test_game.c
)

target_link_libraries(test_game lgx_runtime)

# Install rules
install(TARGETS lgx_runtime
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)
