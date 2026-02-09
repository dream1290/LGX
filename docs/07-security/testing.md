# Security Testing Guide

## Overview

The LGX Runtime includes comprehensive security testing to identify and prevent vulnerabilities. This document describes the security testing tools and procedures.

## Testing Tools

### 1. AFL (American Fuzzy Lop)

AFL is a coverage-guided fuzzer that discovers security vulnerabilities through intelligent mutation of test inputs.

**Purpose:** Test API robustness against malformed inputs

**Setup:**
```bash
cd tests/fuzzing
./build_afl.sh
```

**Running:**
```bash
cd tests/fuzzing
afl-fuzz -i testcases -o findings ./fuzz_api_inputs
```

**What it tests:**
- API parameter validation
- String handling (paths, names)
- Size and alignment validation
- Configuration parsing
- Capability queries
- Health check API
- Frame arena operations
- GPU operations
- Telemetry operations

**Interpreting results:**
- `findings/crashes/`: Inputs that caused crashes
- `findings/hangs/`: Inputs that caused hangs
- `findings/queue/`: Interesting test cases discovered

**Recommended duration:** 24-48 hours for comprehensive coverage

### 2. libFuzzer

libFuzzer is a coverage-guided fuzzer integrated with LLVM, optimized for in-process fuzzing.

**Purpose:** Test allocation patterns and memory management

**Setup:**
```bash
cd tests/fuzzing
./build_libfuzzer.sh
```

**Running:**
```bash
cd tests/fuzzing
./fuzz_allocation_patterns -max_total_time=300
```

**What it tests:**
- Allocation patterns (heap, frame, GPU)
- Free operations
- Reallocation patterns
- Frame reset operations
- Memory operations (read/write)
- Concurrent allocations
- Edge cases (zero size, huge size)

**Advanced usage:**
```bash
# With corpus
mkdir -p corpus
./fuzz_allocation_patterns corpus/ -max_total_time=3600

# Minimize corpus
./fuzz_allocation_patterns -merge=1 corpus_min/ corpus/

# With dictionary
./fuzz_allocation_patterns -dict=allocation.dict corpus/

# Parallel fuzzing
./fuzz_allocation_patterns corpus/ -jobs=8 -workers=8
```

**Recommended duration:** 1-2 hours for initial testing, 24+ hours for thorough testing

### 3. Clang Static Analyzer

The Clang Static Analyzer performs deep static analysis to find bugs without executing code.

**Purpose:** Detect logic errors, memory leaks, and undefined behavior

**Setup:**
```bash
sudo apt-get install clang-tools  # Ubuntu/Debian
sudo dnf install clang-analyzer   # Fedora
```

**Running:**
```bash
./scripts/run_static_analysis.sh
```

**What it checks:**
- Null pointer dereferences
- Use-after-free
- Memory leaks
- Buffer overflows
- Integer overflows
- Dead code
- Logic errors
- API misuse

**Viewing results:**
```bash
scan-view static_analysis_reports/YYYY-MM-DD-HHMMSS/
```

**Interpreting results:**
- High severity: Critical bugs that will cause crashes
- Medium severity: Potential bugs that may cause issues
- Low severity: Code quality issues

### 4. clang-tidy

clang-tidy is a linter that provides additional static analysis checks.

**Purpose:** Code quality and best practices enforcement

**Running:**
```bash
./scripts/run_clang_tidy.sh
```

**What it checks:**
- Bugprone patterns
- CERT secure coding guidelines
- C++ Core Guidelines
- Concurrency issues
- Performance issues
- Portability issues
- Readability issues
- Security issues

**Viewing results:**
```bash
cat clang_tidy_report.txt
```

### 5. Coverity Scan

Coverity Scan is an enterprise-grade static analysis tool that finds critical defects.

**Purpose:** Deep security vulnerability analysis

**Setup:**
1. Register at https://scan.coverity.com/
2. Download Coverity Build Tool
3. Add to PATH: `export PATH=$PATH:/path/to/cov-analysis/bin`

**Running:**
```bash
./scripts/run_coverity_scan.sh
```

**What it checks:**
- Security vulnerabilities
- Concurrency defects
- Resource leaks
- API misuse
- Null pointer dereferences
- Buffer overflows
- Integer overflows
- Use-after-free

**Viewing results:**
```bash
# Local report
open coverity_report/index.html

# Or upload to Coverity Scan service
# (see script output for upload command)
```

**GitHub Integration:**
The project includes a GitHub Actions workflow (`.github/workflows/coverity.yml`) that automatically runs Coverity Scan weekly.

## Testing Workflow

### Development Testing

Run during active development:

1. **Quick checks** (5-10 minutes):
   ```bash
   ./scripts/run_clang_tidy.sh
   ```

2. **Static analysis** (10-15 minutes):
   ```bash
   ./scripts/run_static_analysis.sh
   ```

### Pre-commit Testing

Run before committing code:

1. **All static analysis**:
   ```bash
   ./scripts/run_static_analysis.sh
   ./scripts/run_clang_tidy.sh
   ```

2. **Quick fuzzing** (5 minutes each):
   ```bash
   cd tests/fuzzing
   ./build_libfuzzer.sh
   ./fuzz_allocation_patterns -max_total_time=300
   ```

### Pre-release Testing

Run before releases:

1. **Extended fuzzing** (24-48 hours):
   ```bash
   # AFL
   cd tests/fuzzing
   ./build_afl.sh
   afl-fuzz -i testcases -o findings ./fuzz_api_inputs
   
   # libFuzzer
   ./build_libfuzzer.sh
   ./fuzz_allocation_patterns corpus/ -max_total_time=86400
   ```

2. **Coverity Scan**:
   ```bash
   ./scripts/run_coverity_scan.sh
   ```

3. **Review all findings** and fix critical issues

### Continuous Integration

Automated testing in CI/CD:

1. **On every commit**:
   - clang-tidy
   - Clang Static Analyzer

2. **Weekly**:
   - Coverity Scan (via GitHub Actions)

3. **Before releases**:
   - Extended fuzzing (24+ hours)
   - Full Coverity Scan review

## Common Issues and Fixes

### AFL Issues

**Issue:** "No instrumentation detected"
**Fix:** Ensure AFL is installed and using afl-gcc:
```bash
sudo apt-get install afl
CC=afl-gcc cmake -B build_afl
```

**Issue:** "Suboptimal CPU scaling governor"
**Fix:** Set CPU governor to performance:
```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### libFuzzer Issues

**Issue:** "Sanitizer not enabled"
**Fix:** Ensure AddressSanitizer is enabled:
```bash
clang++ -fsanitize=fuzzer,address ...
```

**Issue:** "Slow execution"
**Fix:** Use optimization level -O1 or -O2:
```bash
clang++ -fsanitize=fuzzer,address -O1 ...
```

### Static Analyzer Issues

**Issue:** "scan-build not found"
**Fix:** Install clang-tools:
```bash
sudo apt-get install clang-tools
```

**Issue:** "Too many false positives"
**Fix:** Disable specific checkers:
```bash
scan-build -disable-checker alpha.deadcode.UnreachableCode ...
```

## Security Testing Metrics

### Coverage Metrics

- **Code coverage:** Target 80%+ line coverage
- **Branch coverage:** Target 70%+ branch coverage
- **Fuzzing coverage:** Track unique code paths discovered

### Defect Metrics

- **Critical defects:** 0 tolerance
- **High severity:** Fix before release
- **Medium severity:** Fix or document
- **Low severity:** Fix or accept

### Fuzzing Metrics

- **Executions per second:** Target 1000+ exec/s
- **Unique crashes:** Investigate all
- **Unique hangs:** Investigate all
- **Corpus size:** Larger is better (more coverage)

## Best Practices

1. **Run static analysis early and often**
   - Catch bugs before they become problems
   - Integrate into development workflow

2. **Fuzz continuously**
   - Run fuzzing overnight or on dedicated machines
   - Maintain and grow corpus over time

3. **Fix critical issues immediately**
   - Security vulnerabilities
   - Memory corruption
   - Crashes

4. **Document accepted risks**
   - False positives
   - Low-severity issues
   - Performance trade-offs

5. **Automate testing**
   - CI/CD integration
   - Scheduled fuzzing runs
   - Automated reporting

6. **Review findings regularly**
   - Weekly review of fuzzing results
   - Monthly review of static analysis
   - Quarterly security audit

## Integration with Development

### Pre-commit Hooks

Add to `.git/hooks/pre-commit`:
```bash
#!/bin/bash
./scripts/run_clang_tidy.sh
if [ $? -ne 0 ]; then
    echo "clang-tidy found issues. Fix before committing."
    exit 1
fi
```

### CI/CD Pipeline

Example GitHub Actions workflow:
```yaml
- name: Static Analysis
  run: ./scripts/run_static_analysis.sh

- name: Quick Fuzzing
  run: |
    cd tests/fuzzing
    ./build_libfuzzer.sh
    ./fuzz_allocation_patterns -max_total_time=300
```

## Resources

- AFL documentation: http://lcamtuf.coredump.cx/afl/
- libFuzzer documentation: https://llvm.org/docs/LibFuzzer.html
- Clang Static Analyzer: https://clang-analyzer.llvm.org/
- clang-tidy: https://clang.llvm.org/extra/clang-tidy/
- Coverity Scan: https://scan.coverity.com/

## References

- Task 9.3: Implement security testing
- Section 9: Security Hardening
- `tests/fuzzing/`: Fuzzing harnesses
- `scripts/`: Analysis scripts
- `.github/workflows/coverity.yml`: CI integration
