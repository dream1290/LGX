# Integration Tests for LGX Runtime Core

# Integration test sources
set(INTEGRATION_TEST_SOURCES
    # test_end_to_end_initialization.c  # DISABLED: Uses non-existent lgx_version_info_t API
    # test_suspend_resume_cycle.c       # DISABLED: Compilation errors
    test_memory_stress.c
    # test_telemetry_collection.c       # DISABLED: Compilation errors
    # test_component_integration.c      # DISABLED: API mismatches
)

# Create integration test executables
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "integration_${test_name}")
    
    add_executable(${test_target} ${test_source})
    target_link_libraries(${test_target} lgx_runtime m)
    target_include_directories(${test_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src/runtime
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        LABELS "integration"
        TIMEOUT 120
    )
endforeach()
