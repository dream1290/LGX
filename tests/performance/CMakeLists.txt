# Performance Tests for LGX Runtime Core

# Performance test sources
set(PERFORMANCE_TEST_SOURCES
    test_initialization_time.c
    test_allocation_latency.c
    test_frame_time_contribution.c
    test_memory_overhead.c
    test_memory_footprint.c
)

# Create performance test executables
foreach(test_source ${PERFORMANCE_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "perf_${test_name}")
    
    add_executable(${test_target} ${test_source})
    target_link_libraries(${test_target} lgx_runtime m)
    target_include_directories(${test_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/tests/common
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        LABELS "performance"
        TIMEOUT 300
    )
endforeach()