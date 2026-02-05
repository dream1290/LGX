# Benchmarks for LGX Runtime Core

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
    target_link_libraries(${benchmark_target} lgx_runtime m)
    target_include_directories(${benchmark_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/tests/common
    )
    
    # Benchmarks are not run as tests by default
    # Use 'make run_benchmarks' to execute them
    add_test(NAME ${benchmark_target} COMMAND ${benchmark_target})
    set_tests_properties(${benchmark_target} PROPERTIES
        LABELS "benchmark"
        TIMEOUT 600
    )
endforeach()

# Benchmark helper library
add_library(benchmark_helpers STATIC
    ../tests/common/test_helpers.c
    ../tests/common/performance_helpers.c
    ../tests/common/statistics.c
    benchmark_framework.c
)

target_include_directories(benchmark_helpers PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/common
    ${CMAKE_SOURCE_DIR}/benchmarks
)

# Link benchmarks with helpers
foreach(benchmark_source ${BENCHMARK_SOURCES})
    get_filename_component(benchmark_name ${benchmark_source} NAME_WE)
    set(benchmark_target "benchmark_${benchmark_name}")
    target_link_libraries(${benchmark_target} benchmark_helpers)
endforeach()