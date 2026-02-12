# Benchmarks for LGX Runtime Core
# Production-grade performance benchmarking suite

message(STATUS "Configuring LGX Runtime Core benchmarks")

# Benchmark framework library
add_library(benchmark_framework STATIC
    benchmark_framework.c
)

target_include_directories(benchmark_framework PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/benchmarks
)

target_link_libraries(benchmark_framework PUBLIC
    m
)

# Benchmark sources
set(BENCHMARK_SOURCES
    benchmark_allocation_throughput.c
    benchmark_memory_patterns.c
    benchmark_hardware_adaptation.c
    benchmark_intent_accuracy.c
    benchmark_telemetry_overhead.c
)

# Create benchmark executables
foreach(benchmark_source ${BENCHMARK_SOURCES})
    get_filename_component(benchmark_name ${benchmark_source} NAME_WE)
    set(benchmark_target "benchmark_${benchmark_name}")
    
    add_executable(${benchmark_target} ${benchmark_source})
    
    target_link_libraries(${benchmark_target} 
        lgx_runtime 
        benchmark_framework
        m
    )
    
    target_include_directories(${benchmark_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/benchmarks
    )
    
    # Set optimization flags for benchmarks
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${benchmark_target} PRIVATE
            -O3
            -march=native
            -mtune=native
        )
    endif()
    
    # Benchmarks are labeled for selective execution
    # Run with: ctest -L benchmark
    add_test(NAME ${benchmark_target} COMMAND ${benchmark_target})
    set_tests_properties(${benchmark_target} PROPERTIES
        LABELS "benchmark"
        TIMEOUT 600
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    message(STATUS "  Configured benchmark: ${benchmark_target}")
endforeach()

# Custom target to run all benchmarks
add_custom_target(run_benchmarks
    COMMAND ${CMAKE_CTEST_COMMAND} -L benchmark --output-on-failure
    DEPENDS ${BENCHMARK_SOURCES}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all benchmarks..."
)

message(STATUS "Benchmarks configured successfully")
message(STATUS "  Run benchmarks with: make run_benchmarks")
message(STATUS "  Or: ctest -L benchmark")