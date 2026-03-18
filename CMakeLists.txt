cmake_minimum_required(VERSION 3.16)
project(lgx_runtime VERSION 1.0.1 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for Phase 1 production build
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -fPIC \
    -Wredundant-decls \
    -Wshadow")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined -mavx2")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG -march=native")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG -march=native")

# Find required packages
find_package(Threads REQUIRED)

# Optional: Find Vulkan for GPU memory pool
find_package(Vulkan QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ALSA QUIET alsa)
endif()
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
    src/runtime/lgx_telemetry_process.c
    src/runtime/lgx_error_handler.c
    src/runtime/lgx_health_monitor.c
    src/runtime/lgx_leak_detector.c
    src/runtime/lgx_memory_monitor.c
    src/runtime/lgx_performance_counters.c
    src/runtime/lgx_chaos_testing.c
    src/runtime/lgx_trace_events.c
    src/runtime/lgx_namespace_isolation.c
    src/runtime/lgx_library_manifest.c
    src/runtime/lgx_input_validation.c
    src/runtime/lgx_memory_safety.c
    src/runtime/lgx_resource_limits.c
    
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

# ═══════════════════════════════════════════════════════════════════════════
# LGX Graphics Module v1.2
# ═══════════════════════════════════════════════════════════════════════════
if(Vulkan_FOUND)
    add_library(lgx_graphics SHARED src/graphics/lgx_graphics.c)
    target_include_directories(lgx_graphics PUBLIC include)
    target_include_directories(lgx_graphics PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(lgx_graphics lgx_runtime ${Vulkan_LIBRARY} Threads::Threads)
    set_target_properties(lgx_graphics PROPERTIES
        VERSION 1.2.0
        SOVERSION 1
    )
    # Suppress strict-prototype warnings for graphics module (uses designated inits heavily)
    target_compile_options(lgx_graphics PRIVATE -Wno-missing-prototypes)

    install(TARGETS lgx_graphics LIBRARY DESTINATION lib)
    install(FILES include/lgx_graphics.h DESTINATION include)
    message(STATUS "LGX Graphics Module v1.2 enabled")
else()
    message(STATUS "LGX Graphics Module v1.2 disabled (Vulkan not found)")
endif()

# ═══════════════════════════════════════════════════════════════════════════
# LGX Input Module v1.3
# ═══════════════════════════════════════════════════════════════════════════
add_library(lgx_input SHARED src/input/lgx_input.c)
target_include_directories(lgx_input PUBLIC include)
target_link_libraries(lgx_input lgx_runtime)
set_target_properties(lgx_input PROPERTIES
    VERSION 1.3.0
    SOVERSION 1
)
target_compile_options(lgx_input PRIVATE -Wno-missing-prototypes)

install(TARGETS lgx_input LIBRARY DESTINATION lib)
install(FILES include/lgx_input.h DESTINATION include)
message(STATUS "LGX Input Module v1.3 enabled")

# ═══════════════════════════════════════════════════════════════════════════
# LGX Audio Module v1.4
# ═══════════════════════════════════════════════════════════════════════════
if(ALSA_FOUND)
    add_library(lgx_audio SHARED src/audio/lgx_audio.c)
    target_include_directories(lgx_audio PUBLIC include)
    target_include_directories(lgx_audio PRIVATE ${ALSA_INCLUDE_DIRS})
    target_link_libraries(lgx_audio lgx_runtime ${ALSA_LIBRARIES} m)
    set_target_properties(lgx_audio PROPERTIES
        VERSION 1.4.0
        SOVERSION 1
    )
    target_compile_options(lgx_audio PRIVATE -Wno-missing-prototypes)

    install(TARGETS lgx_audio LIBRARY DESTINATION lib)
    install(FILES include/lgx_audio.h DESTINATION include)
    message(STATUS "LGX Audio Module v1.4 enabled")
else()
    message(STATUS "LGX Audio Module v1.4 disabled (ALSA not found)")
endif()

# ═══════════════════════════════════════════════════════════════════════════
# LGX Profiling Module v1.5
# ═══════════════════════════════════════════════════════════════════════════
add_library(lgx_profile SHARED src/profile/lgx_profile.c)
target_include_directories(lgx_profile PUBLIC include)
target_link_libraries(lgx_profile lgx_runtime)
set_target_properties(lgx_profile PROPERTIES
    VERSION 1.5.0
    SOVERSION 1
)
target_compile_options(lgx_profile PRIVATE -Wno-missing-prototypes)

install(TARGETS lgx_profile LIBRARY DESTINATION lib)
install(FILES include/lgx_profile.h DESTINATION include)
message(STATUS "LGX Profiling Module v1.5 enabled")

# ═══════════════════════════════════════════════════════════════════════════
# LGX Networking Module v2.0
# ═══════════════════════════════════════════════════════════════════════════
add_library(lgx_net SHARED src/net/lgx_net.c)
target_include_directories(lgx_net PUBLIC include)
target_link_libraries(lgx_net lgx_runtime)
set_target_properties(lgx_net PROPERTIES
    VERSION 2.0.0
    SOVERSION 2
)
target_compile_options(lgx_net PRIVATE -Wno-missing-prototypes)

install(TARGETS lgx_net LIBRARY DESTINATION lib)
install(FILES include/lgx_net.h DESTINATION include)
message(STATUS "LGX Networking Module v2.0 enabled")

# ═══════════════════════════════════════════════════════════════════════════
# LGX Asset Pipeline v2.1
# ═══════════════════════════════════════════════════════════════════════════
add_library(lgx_asset SHARED src/asset/lgx_asset.c)
target_include_directories(lgx_asset PUBLIC include)
target_link_libraries(lgx_asset lgx_runtime pthread)
set_target_properties(lgx_asset PROPERTIES
    VERSION 2.1.0
    SOVERSION 2
)
target_compile_options(lgx_asset PRIVATE -Wno-missing-prototypes)

install(TARGETS lgx_asset LIBRARY DESTINATION lib)
install(FILES include/lgx_asset.h DESTINATION include)
message(STATUS "LGX Asset Pipeline v2.1 enabled")

# ═══════════════════════════════════════════════════════════════════════════
# LGX Tooling v2.2
# ═══════════════════════════════════════════════════════════════════════════
add_library(lgx_tools SHARED src/tools/lgx_tools.c)
target_include_directories(lgx_tools PUBLIC include)
target_link_libraries(lgx_tools lgx_runtime m)
set_target_properties(lgx_tools PROPERTIES
    VERSION 2.2.0
    SOVERSION 2
)
target_compile_options(lgx_tools PRIVATE -Wno-missing-prototypes)

install(TARGETS lgx_tools LIBRARY DESTINATION lib)
install(FILES include/lgx_tools.h DESTINATION include)
message(STATUS "LGX Tooling v2.2 enabled")

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

# Phase 3 hardening: strict prototype checks on the library only
# (tests use old-style declarations so these flags would break them)
target_compile_options(lgx_runtime PRIVATE
    -Wstrict-prototypes
    -Wmissing-prototypes
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
    set_tests_properties(${test_name} PROPERTIES
        LABELS "phase0"
        TIMEOUT 300  # 5 minutes for AddressSanitizer overhead
        ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
    )
endforeach()

# Phase 1 Unit Tests
add_subdirectory(tests/unit)

# Phase 1 Integration Tests
add_subdirectory(tests/integration)

# ABI Compatibility Tests
add_subdirectory(tests/abi)

# Performance Tests
add_subdirectory(tests/performance)

# Failure Injection Tests
add_subdirectory(tests/failure_injection)

# Benchmarks
add_subdirectory(benchmarks)

# Graphics Module Tests
if(Vulkan_FOUND)
    add_executable(test_graphics_basic tests/graphics/test_graphics_basic.c)
    target_link_libraries(test_graphics_basic lgx_graphics lgx_runtime ${Vulkan_LIBRARY} m)
    target_include_directories(test_graphics_basic PRIVATE ${Vulkan_INCLUDE_DIRS})
    add_test(NAME graphics_test_basic COMMAND test_graphics_basic)
    set_tests_properties(graphics_test_basic PROPERTIES
        LABELS "graphics"
        TIMEOUT 30
    )
endif()

# Input Module Tests
add_executable(test_input_basic tests/input/test_input_basic.c)
target_link_libraries(test_input_basic lgx_input lgx_runtime)
add_test(NAME input_test_basic COMMAND test_input_basic)
set_tests_properties(input_test_basic PROPERTIES
    LABELS "input"
    TIMEOUT 30
)

# Audio Module Tests
if(ALSA_FOUND)
    add_executable(test_audio_basic tests/audio/test_audio_basic.c)
    target_link_libraries(test_audio_basic lgx_audio lgx_runtime ${ALSA_LIBRARIES} m)
    target_include_directories(test_audio_basic PRIVATE ${ALSA_INCLUDE_DIRS})
    add_test(NAME audio_test_basic COMMAND test_audio_basic)
    set_tests_properties(audio_test_basic PROPERTIES
        LABELS "audio"
        TIMEOUT 30
    )
endif()

# Profiling Module Tests
add_executable(test_profile_basic tests/profile/test_profile_basic.c)
target_link_libraries(test_profile_basic lgx_profile lgx_runtime)
add_test(NAME profile_test_basic COMMAND test_profile_basic)
set_tests_properties(profile_test_basic PROPERTIES
    LABELS "profile"
    TIMEOUT 30
)

# Networking Module Tests
add_executable(test_net_basic tests/net/test_net_basic.c)
target_link_libraries(test_net_basic lgx_net lgx_runtime m)
add_test(NAME net_test_basic COMMAND test_net_basic)
set_tests_properties(net_test_basic PROPERTIES
    LABELS "net"
    TIMEOUT 30
)

# Asset Pipeline Tests
add_executable(test_asset_basic tests/asset/test_asset_basic.c)
target_link_libraries(test_asset_basic lgx_asset lgx_runtime)
add_test(NAME asset_test_basic COMMAND test_asset_basic)
set_tests_properties(asset_test_basic PROPERTIES
    LABELS "asset"
    TIMEOUT 30
)

# Tooling Tests
add_executable(test_tools_basic tests/tools/test_tools_basic.c)
target_link_libraries(test_tools_basic lgx_tools lgx_runtime m)
add_test(NAME tools_test_basic COMMAND test_tools_basic)
set_tests_properties(tools_test_basic PROPERTIES
    LABELS "tools"
    TIMEOUT 30
)

# Demo applications
# add_subdirectory(demo)  # Commented out - demo directory removed

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

# run_benchmarks target is defined in benchmarks/CMakeLists.txt

# ──────────────────── LGX Threading Module (v1.1) ────────────────────

set(LGX_THREADING_SOURCES
    src/threading/lgx_threading.c
)

add_library(lgx_threading SHARED ${LGX_THREADING_SOURCES})

set_target_properties(lgx_threading PROPERTIES
    VERSION 1.1.0
    SOVERSION 1
    LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/lgx_threading.map"
)

target_include_directories(lgx_threading
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/src/threading
)

target_link_libraries(lgx_threading
    Threads::Threads
)

target_compile_options(lgx_threading PRIVATE
    -Wstrict-prototypes
    -Wmissing-prototypes
)

# Threading tests
add_executable(test_threading_basic tests/threading/test_threading_basic.c)
target_include_directories(test_threading_basic PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_threading_basic lgx_threading Threads::Threads)
add_test(NAME test_threading_basic COMMAND test_threading_basic)
set_tests_properties(test_threading_basic PROPERTIES
    LABELS "threading"
    TIMEOUT 30
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)

add_executable(test_threading_stress tests/threading/test_threading_stress.c)
target_include_directories(test_threading_stress PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_threading_stress lgx_threading Threads::Threads)
add_test(NAME test_threading_stress COMMAND test_threading_stress)
set_tests_properties(test_threading_stress PROPERTIES
    LABELS "threading"
    TIMEOUT 60
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)

add_executable(test_threading_fiber tests/threading/test_threading_fiber.c)
target_include_directories(test_threading_fiber PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_threading_fiber lgx_threading Threads::Threads)
add_test(NAME test_threading_fiber COMMAND test_threading_fiber)
set_tests_properties(test_threading_fiber PROPERTIES
    LABELS "threading"
    TIMEOUT 30
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)

add_executable(benchmark_threading tests/threading/benchmark_threading.c)
target_include_directories(benchmark_threading PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(benchmark_threading lgx_threading Threads::Threads)
add_test(NAME benchmark_threading COMMAND benchmark_threading)
set_tests_properties(benchmark_threading PROPERTIES
    LABELS "threading;benchmark"
    TIMEOUT 60
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)

add_executable(test_threading_advanced tests/threading/test_threading_advanced.c)
target_include_directories(test_threading_advanced PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_threading_advanced lgx_threading Threads::Threads)
add_test(NAME test_threading_advanced COMMAND test_threading_advanced)
set_tests_properties(test_threading_advanced PROPERTIES
    LABELS "threading"
    TIMEOUT 30
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)


add_custom_target(run_threading_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R "threading"
    COMMENT "Running threading module tests"
)