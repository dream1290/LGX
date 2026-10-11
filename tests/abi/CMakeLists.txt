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

# C++ consumability gate: the public header must compile and link as C++.
# The top-level project() declares LANGUAGES C only, so C++ is enabled here
# purely for this gate target.
enable_language(CXX)

add_executable(abi_test_cpp_header_compiles test_cpp_header_compiles.cpp)
target_link_libraries(abi_test_cpp_header_compiles lgx_runtime)
target_include_directories(abi_test_cpp_header_compiles PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(abi_test_cpp_header_compiles PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_options(abi_test_cpp_header_compiles PRIVATE -Wall -Wextra -Werror)

add_test(NAME abi_test_cpp_header_compiles COMMAND abi_test_cpp_header_compiles)
set_tests_properties(abi_test_cpp_header_compiles PROPERTIES
    LABELS "abi"
    TIMEOUT 30
)

# Install ABI test script
install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/lgx-abi-test-matrix.sh
    DESTINATION bin
    RENAME lgx-abi-test-matrix
)
