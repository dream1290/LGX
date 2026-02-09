# ABI Compatibility Tests for LGX Runtime Core

# ABI test sources
set(ABI_TEST_SOURCES
    test_game_v1_0.c
    test_struct_evolution.c
    test_symbol_versioning.c
)

# Create ABI test executables
foreach(test_source ${ABI_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} lgx_runtime m dl)
    target_include_directories(${test_name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime
    )
    
    add_test(NAME abi_${test_name} COMMAND ${test_name})
    set_tests_properties(abi_${test_name} PROPERTIES
        LABELS "abi"
        TIMEOUT 60
    )
endforeach()

# Custom target to run all ABI tests
add_custom_target(run_abi_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L abi
    DEPENDS ${ABI_TEST_SOURCES}
    COMMENT "Running ABI compatibility tests"
)

# Install ABI test script
install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/lgx-abi-test-matrix.sh
    DESTINATION bin
    RENAME lgx-abi-test-matrix
)
