# Security Documentation

Security threat model, audit preparation, and implementation details.

## Contents

- [threat-model.md](threat-model.md) - Security threat model and attack surface
- [audit-preparation.md](audit-preparation.md) - Third-party security audit preparation
- [checklist.md](checklist.md) - Security checklist
- [testing.md](testing.md) - Security testing guide
- [implementations/](implementations/) - Security implementation details

## Security Features

### Input Validation
- Null pointer checks on all API functions
- Size bounds validation
- String length validation and truncation
- Enum range validation

### Memory Safety
- Guard pages after allocations (debug builds)
- Memory canaries to detect corruption
- Delayed reclamation (3-frame) to prevent use-after-free
- Allocation tracking to prevent double-free

### Namespace Isolation
- Isolated mount namespace for libraries
- Library version validation at startup
- Pinned library versions

## Security Testing

- **Fuzzing**: AFL and libFuzzer for API inputs and allocation patterns
- **Static Analysis**: Clang Static Analyzer and Coverity Scan
- **Failure Injection**: OOM, GPU timeout, filesystem full, TOCTOU races

See [testing.md](testing.md) for details.
