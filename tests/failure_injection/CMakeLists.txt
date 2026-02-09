# Failure Injection Tests for LGX Runtime Core

# Test sources
set(FAILURE_INJECTION_TESTS
    test_oom_injection.c
    test_gpu_timeout.c
    test_library_version_mismatch.c
    test_telemetry_crash.c
    test_filesystem_full.c
    test_toctou_races.c
)

# Create test executables
foreach(test_source ${FAILURE_INJECTION_TESTS})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} lgx_runtime m pthread)
    target_include_directories(${test_name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime
    )
    
    # Add as CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        LABELS "failure_injection"
        TIMEOUT 60
    )
endforeach()

# Custom target to run all failure injection tests
add_custom_target(run_failure_injection_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L failure_injection
    DEPENDS ${FAILURE_INJECTION_TESTS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running failure injection tests..."
)
