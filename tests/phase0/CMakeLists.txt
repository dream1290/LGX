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

# Chaos deadlock regesstion gate (Task B4). The 20s timeout is the point:
# pre-fix the decision call hangs forever; a generic 300s timeout would be
# the wrong (slow) failure mode.
add_executable(test_chaos_deadlock test_chaos_deadlock.c)
target_link_libraries(test_chaos_deadlock lgx_runtime m)
add_test(NAME test_chaos_deadlock COMMAND test_chaos_deadlock)
set_tests_properties(test_chaos_deadlock PROPERTIES
    LABELS "phase0"
    TIMEOUT 20
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)
# The NUMA allocation-cost timing verdict is a benchmark observation, not a
# correctness property, so it is opt-in and off on the default run. Functional
# assertions always run. To enforce the timing threshold, run ctest under an
# environment that sets the variable (ctest passes its own environment on):
#   cmake -E env LGX_NUMA_PERF=1 ctest -R test_csf2_numa_awareness
# or just run the binary with LGX_NUMA_PERF=1 in the environment.
# NOTE: `ctest --overwrite Environment=...` does NOT work here. --overwrite takes
# a CTest configuration FILE; it silently ignores this argument and runs the
# default, unarmed path.
set_tests_properties(test_csf2_numa_awareness PROPERTIES
    LABELS "phase0"
    TIMEOUT 300
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)
