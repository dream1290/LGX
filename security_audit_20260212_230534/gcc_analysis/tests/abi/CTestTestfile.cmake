# CMake generated Testfile for 
# Source directory: /home/karl/Projects/LGX/tests/abi
# Build directory: /home/karl/Projects/LGX/security_audit_20260212_230534/gcc_analysis/tests/abi
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(abi_test_game_v1_0 "/home/karl/Projects/LGX/security_audit_20260212_230534/gcc_analysis/tests/abi/test_game_v1_0")
set_tests_properties(abi_test_game_v1_0 PROPERTIES  LABELS "abi" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/abi/CMakeLists.txt;21;add_test;/home/karl/Projects/LGX/tests/abi/CMakeLists.txt;0;")
add_test(abi_test_struct_evolution "/home/karl/Projects/LGX/security_audit_20260212_230534/gcc_analysis/tests/abi/test_struct_evolution")
set_tests_properties(abi_test_struct_evolution PROPERTIES  LABELS "abi" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/abi/CMakeLists.txt;21;add_test;/home/karl/Projects/LGX/tests/abi/CMakeLists.txt;0;")
add_test(abi_test_symbol_versioning "/home/karl/Projects/LGX/security_audit_20260212_230534/gcc_analysis/tests/abi/test_symbol_versioning")
set_tests_properties(abi_test_symbol_versioning PROPERTIES  LABELS "abi" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/abi/CMakeLists.txt;21;add_test;/home/karl/Projects/LGX/tests/abi/CMakeLists.txt;0;")
