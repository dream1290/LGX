# CMake generated Testfile for 
# Source directory: /home/karl/Projects/LGX/tests/integration
# Build directory: /home/karl/Projects/LGX/security_audit_20260212_230534/gcc_analysis/tests/integration
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(integration_test_memory_stress "/home/karl/Projects/LGX/security_audit_20260212_230534/gcc_analysis/tests/integration/integration_test_memory_stress")
set_tests_properties(integration_test_memory_stress PROPERTIES  LABELS "integration" TIMEOUT "120" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/integration/CMakeLists.txt;23;add_test;/home/karl/Projects/LGX/tests/integration/CMakeLists.txt;0;")
