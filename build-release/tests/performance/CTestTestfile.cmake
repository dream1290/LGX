# CMake generated Testfile for 
# Source directory: /home/karl/Projects/LGX/tests/performance
# Build directory: /home/karl/Projects/LGX/build-release/tests/performance
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(perf_test_initialization_time "/home/karl/Projects/LGX/build-release/tests/performance/perf_test_initialization_time")
set_tests_properties(perf_test_initialization_time PROPERTIES  LABELS "performance" TIMEOUT "300" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;24;add_test;/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;0;")
add_test(perf_test_allocation_latency "/home/karl/Projects/LGX/build-release/tests/performance/perf_test_allocation_latency")
set_tests_properties(perf_test_allocation_latency PROPERTIES  LABELS "performance" TIMEOUT "300" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;24;add_test;/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;0;")
add_test(perf_test_frame_time_contribution "/home/karl/Projects/LGX/build-release/tests/performance/perf_test_frame_time_contribution")
set_tests_properties(perf_test_frame_time_contribution PROPERTIES  LABELS "performance" TIMEOUT "300" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;24;add_test;/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;0;")
add_test(perf_test_memory_overhead "/home/karl/Projects/LGX/build-release/tests/performance/perf_test_memory_overhead")
set_tests_properties(perf_test_memory_overhead PROPERTIES  LABELS "performance" TIMEOUT "300" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;24;add_test;/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;0;")
add_test(perf_test_memory_footprint "/home/karl/Projects/LGX/build-release/tests/performance/perf_test_memory_footprint")
set_tests_properties(perf_test_memory_footprint PROPERTIES  LABELS "performance" TIMEOUT "300" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;24;add_test;/home/karl/Projects/LGX/tests/performance/CMakeLists.txt;0;")
