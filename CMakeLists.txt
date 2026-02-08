cmake_minimum_required(VERSION 3.16)
project(lgx_runtime VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for Phase 1 production build
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -fPIC")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG -march=native")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Optional: Find Vulkan for GPU memory pool
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_LIBRARY}")
    add_definitions(-DHAVE_VULKAN=1)
else()
    message(STATUS "Vulkan not found - GPU memory pool features will be disabled")
    add_definitions(-DHAVE_VULKAN=0)
endif()

# Optional: Find pkg-config
find_package(PkgConfig QUIET)
if(NOT PkgConfig_FOUND)
    message(STATUS "pkg-config not found - some optional features may be disabled")
endif()

# Optional: Find jemalloc for fallback allocator
find_library(JEMALLOC_LIB jemalloc)
if(JEMALLOC_LIB)
    message(STATUS "Found jemalloc: ${JEMALLOC_LIB}")
    add_definitions(-DHAVE_JEMALLOC=1)
else()
    message(STATUS "jemalloc not found - using system malloc for fallback")
    add_definitions(-DHAVE_JEMALLOC=0)
endif()

# Include directories
include_directories(include)
include_directories(include/lgx)

# Phase 1 Runtime Core Library Sources
set(LGX_RUNTIME_SOURCES
    src/runtime/lgx_runtime_core.c
    src/runtime/lgx_memory_manager.c
    src/runtime/lgx_lockfree_pool.c
    src/runtime/lgx_simd_ops.c
    src/runtime/lgx_hugepages.c
    src/runtime/lgx_frame_arena.c
    src/runtime/lgx_hardware_adapter.c
    src/runtime/lgx_capability_detector.c
    src/runtime/lgx_lifecycle_manager.c
    src/runtime/lgx_platform_services.c
    src/runtime/lgx_telemetry.c
    src/runtime/lgx_error_handler.c
    src/runtime/lgx_health_monitor.c
    src/runtime/lgx_performance_counters.c
    src/runtime/lgx_chaos_testing.c
    src/runtime/lgx_trace_events.c
    
    # Legacy Phase 0 sources (temporarily disabled)
    # src/lgx_runtime.c
    src/lgx_allocator_prototype.c
)

# Add GPU pool if Vulkan is available
if(Vulkan_FOUND)
    list(APPEND LGX_RUNTIME_SOURCES src/runtime/lgx_gpu_pool.c)
    list(APPEND LGX_RUNTIME_SOURCES src/runtime/lgx_gpu_capability.c)
endif()

# Add persistent heap allocator (Month 3)
list(APPEND LGX_RUNTIME_SOURCES src/runtime/lgx_persistent_heap.c)

# Add intent allocator (Task 3.4)
list(APPEND LGX_RUNTIME_SOURCES src/runtime/lgx_intent_allocator.c)

# Create the main runtime library
add_library(lgx_runtime SHARED ${LGX_RUNTIME_SOURCES})

# Set library properties with symbol versioning
set_target_properties(lgx_runtime PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "include/lgx_runtime.h;include/lgx_types.h;include/lgx_version.h;include/lgx_integration.h"
    LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/lgx_runtime.map"
)

# Link libraries
target_link_libraries(lgx_runtime 
    Threads::Threads
    m  # Math library
    dl # Dynamic loading
)

# Add Vulkan if available
if(Vulkan_FOUND)
    target_link_libraries(lgx_runtime ${Vulkan_LIBRARY})
    target_include_directories(lgx_runtime PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# Add jemalloc if available
if(JEMALLOC_LIB)
    target_link_libraries(lgx_runtime ${JEMALLOC_LIB})
    target_compile_definitions(lgx_runtime PRIVATE HAVE_JEMALLOC=1)
else()
    target_compile_definitions(lgx_runtime PRIVATE HAVE_JEMALLOC=0)
endif()

# Compile definitions
target_compile_definitions(lgx_runtime PRIVATE
    LGX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LGX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LGX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    LGX_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)

# Installation configuration
install(TARGETS lgx_runtime
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# Install internal headers separately
install(FILES include/lgx/lgx_runtime_internal.h
    DESTINATION include/lgx
)

# Install pkg-config file
configure_file(
    ${CMAKE_SOURCE_DIR}/lgx_runtime.pc.in
    ${CMAKE_BINARY_DIR}/lgx_runtime.pc
    @ONLY
)
install(FILES ${CMAKE_BINARY_DIR}/lgx_runtime.pc
    DESTINATION lib/pkgconfig
)

# Testing configuration
enable_testing()

# Phase 0 tests (moved to phase0 subdirectory)
file(GLOB PHASE0_TESTS tests/phase0/*.c)
foreach(test_file ${PHASE0_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} lgx_runtime m)
    
    # Add Vulkan for GPU pool tests
    if(Vulkan_FOUND AND test_name MATCHES "gpu")
        target_link_libraries(${test_name} ${Vulkan_LIBRARY})
        target_include_directories(${test_name} PRIVATE ${Vulkan_INCLUDE_DIRS})
    endif()
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Phase 1 Unit Tests (disabled until tests are implemented)
# add_subdirectory(tests/unit)

# Phase 1 Integration Tests (disabled until tests are implemented)
# add_subdirectory(tests/integration)

# Phase 1 Performance Tests (disabled until tests are implemented)
# add_subdirectory(tests/performance)

# Benchmarks (disabled until benchmarks are implemented)
# add_subdirectory(benchmarks)

# Custom targets for different test suites
add_custom_target(run_phase0_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "test_csf|test_performance|test_tiered|test_intent|test_hardware"
    COMMENT "Running Phase 0 validation tests"
)

add_custom_target(run_unit_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "unit_"
    COMMENT "Running unit tests"
)

add_custom_target(run_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "integration_"
    COMMENT "Running integration tests"
)

add_custom_target(run_performance_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "perf_"
    COMMENT "Running performance tests"
)

add_custom_target(run_benchmarks
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "benchmark_"
    COMMENT "Running benchmarks"
)