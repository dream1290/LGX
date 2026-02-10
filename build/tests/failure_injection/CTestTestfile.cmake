# CMake generated Testfile for 
# Source directory: /home/karl/Projects/LGX/tests/failure_injection
# Build directory: /home/karl/Projects/LGX/build/tests/failure_injection
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_library_version_mismatch "/home/karl/Projects/LGX/build/tests/failure_injection/test_library_version_mismatch")
set_tests_properties(test_library_version_mismatch PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
