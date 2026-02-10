# CMake generated Testfile for 
# Source directory: /home/karl/Projects/LGX/tests/failure_injection
# Build directory: /home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_oom_injection "/home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection/test_oom_injection")
set_tests_properties(test_oom_injection PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
add_test(test_gpu_timeout "/home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection/test_gpu_timeout")
set_tests_properties(test_gpu_timeout PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
add_test(test_library_version_mismatch "/home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection/test_library_version_mismatch")
set_tests_properties(test_library_version_mismatch PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
add_test(test_telemetry_crash "/home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection/test_telemetry_crash")
set_tests_properties(test_telemetry_crash PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
add_test(test_filesystem_full "/home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection/test_filesystem_full")
set_tests_properties(test_filesystem_full PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
add_test(test_toctou_races "/home/karl/Projects/LGX/security_audit_20260210_124057/gcc_analysis/tests/failure_injection/test_toctou_races")
set_tests_properties(test_toctou_races PROPERTIES  LABELS "failure_injection" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;25;add_test;/home/karl/Projects/LGX/tests/failure_injection/CMakeLists.txt;0;")
