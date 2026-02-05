# Integration Tests for LGX Runtime Core

# Integration test sources
set(INTEGRATION_TEST_SOURCES
    test_end_to_end_initialization.c
    test_suspend_resume_cycle.c
    test_memory_stress.c
    test_telemetry_collection.c
    test_component_integration.c
    test_abi_compatibility.c
    test_hardware_diversity.c
    test_failure_injection.c
)

# Create integration test executables
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "integration_${test_name}")
    
    add_executable(${test_target} ${test_source})
    target_link_libraries(${test_target} lgx_runtime m)
    target_include_directories(${test_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/tests/common
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        LABELS "integration"
        TIMEOUT 120
    )
endforeach()

# Integration test helper library
add_library(integration_test_helpers STATIC
    ../common/test_helpers.c
    ../common/system_helpers.c
    ../common/chaos_injection.c
)

target_include_directories(integration_test_helpers PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/common
)

# Link integration tests with helpers
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "integration_${test_name}")
    target_link_libraries(${test_target} integration_test_helpers)
endforeach()