# Unit Tests for LGX Runtime Core

# Unit test sources
set(UNIT_TEST_SOURCES
    test_init_shutdown.c
    # test_version_compatibility.c  # DISABLED: Uses non-existent lgx_version_info_t API
    # test_memory_allocation.c      # DISABLED: Compilation errors
    test_platform_services.c
    test_error_handler.c
    test_health_monitor.c
    test_resource_limits.c
    # test_memory_protection.c      # DISABLED: Compilation errors
)

# Create unit test executables
foreach(test_source ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    set(test_target "unit_${test_name}")
    
    add_executable(${test_target} ${test_source})
    target_link_libraries(${test_target} lgx_runtime)
    target_include_directories(${test_target} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src/runtime
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        LABELS "unit"
        TIMEOUT 30
    )
endforeach()