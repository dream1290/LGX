# Explicit target for the NUMA test.
#
# Unlike the generic phase0 glob in the root CMakeLists.txt, this test is
# conditional: it requires libnuma, and it must still exit 0 (reporting SKIP)
# on machines where libnuma is absent, so CI is portable.

if(NUMA_LIBRARY)
    add_executable(test_csf2_numa_awareness test_csf2_numa_awareness.c)
    target_link_libraries(test_csf2_numa_awareness lgx_runtime m ${NUMA_LIBRARY})
    target_compile_definitions(test_csf2_numa_awareness PRIVATE LGX_HAVE_NUMA=1)
else()
    # No libnuma: build the same source without the numa symbols. The test
    # body is guarded by LGX_HAVE_NUMA and reports SKIP, exiting 0, so CI is
    # portable across machines with and without libnuma.
    add_executable(test_csf2_numa_awareness test_csf2_numa_awareness.c)
    target_link_libraries(test_csf2_numa_awareness lgx_runtime m)
    target_compile_definitions(test_csf2_numa_awareness PRIVATE LGX_HAVE_NUMA=0)
endif()

add_test(NAME test_csf2_numa_awareness COMMAND test_csf2_numa_awareness)
set_tests_properties(test_csf2_numa_awareness PROPERTIES
    LABELS "phase0"
    TIMEOUT 300
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)
