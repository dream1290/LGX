# Unit Tests for LGX Runtime Core

# Unit test sources
set(UNIT_TEST_SOURCES
    test_memory_manager.c
    test_hardware_adapter.c
    test_version_manager.c
    test_capability_detector.c
    test_lifecycle_manager.c
    test_platform_services.c
    test_error_handler.c
    test_health_monitor.c
)

# Create unit test executables
foreach(test_source ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "unit_${test_name}")
    
    add_executable(${test_target} ${test_source})
    target_link_libraries(${test_target} lgx_runtime)
    target_include_directories(${test_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/tests/common
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        LABELS "unit"
        TIMEOUT 30
    )
endforeach()

# Unit test helper library
add_library(unit_test_helpers STATIC
    ../common/test_helpers.c
    ../common/mock_system.c
)

target_include_directories(unit_test_helpers PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests/common
)

# Link unit tests with helpers
foreach(test_source ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "unit_${test_name}")
    target_link_libraries(${test_target} unit_test_helpers)
endforeach()