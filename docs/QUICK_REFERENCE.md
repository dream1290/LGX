# LGX Documentation Quick Reference

Quick links to commonly accessed documentation.

## Platform Overview
- [Platform Vision](LGX_PLATFORM_VISION.md) - Mission, strategy, and roadmap
- [Platform Architecture](LGX_PLATFORM_ARCHITECTURE.md) - Module design and ABI principles

## Getting Started
- [Installation & Quick Start](01-getting-started/README.md)

## API Documentation
- [Runtime Core API](02-api-reference/lgx_runtime_api.md) - Memory management, initialization, lifecycle
- [Threading API](02-api-reference/lgx_threading_api.md) - Concurrency primitives
- [Audio API](02-api-reference/lgx_audio_api.md) - 3D spatial audio
- [Asset API](02-api-reference/lgx_asset_api.md) - Asset loading and hot-reload
- [Graphics API](02-api-reference/lgx_graphics_api.md) - Rendering
- [Input API](02-api-reference/lgx_input_api.md) - Input handling
- [Networking API](02-api-reference/lgx_net_api.md) - Network primitives
- [Profiling API](02-api-reference/lgx_profile_api.md) - Performance analysis
- [Tools API](02-api-reference/lgx_tools_api.md) - Development utilities

## Integration
- [Integration Guide](04-integration/lgx_runtime_integration_guide.md) - How to integrate LGX
- [SuperTuxKart Integration](04-integration/INTEGRATION_SUMMARY.md) - Real-world example

## Architecture
- [System Architecture](03-architecture/lgx_runtime_architecture.md) - Complete architecture overview
- [Architecture Overview](03-architecture/README.md) - Component design

## Performance
- [Performance Targets](05-performance/targets.md) - Performance goals
- [Benchmarks](05-performance/benchmarks.md) - Benchmark suite
- [Real-World Performance](05-performance/STK_LGX_REAL_PERFORMANCE.md) - SuperTuxKart results
- [malloc Comparison](05-performance/MALLOC_VS_LGX_COMPARISON.md) - vs standard allocator

## Testing
- [Test Suite](06-testing/complete-suite.md) - Complete test coverage
- [Chaos Testing](06-testing/chaos-testing.md) - Stress testing

## Security
- [Security Overview](07-security/README.md) - Security model
- [Threat Model](07-security/threat-model.md) - Security analysis
- [Security Checklist](07-security/checklist.md) - Security audit checklist

## Operations
- [Hardware Compatibility](08-operations/hardware-compatibility.md) - Supported hardware
- [Graceful Degradation](08-operations/graceful-degradation.md) - Fallback behavior
- [Telemetry Privacy](08-operations/telemetry-privacy.md) - Privacy policy
- [Video Recording Guide](08-operations/VIDEO_RECORDING_GUIDE.md) - Demo recording

## Development
- [Project History](09-development/project-history.md) - Development timeline
- [Implementation Plan](09-development/implementation_plan.md) - Refactoring plan
- [Current Tasks](09-development/task.md) - Active work
- [Platform Strategy](09-development/platform-strategy.md) - Platform vision and positioning
- [Development Strategy](09-development/development-strategy.md) - Architecture-first approach
- [Go-to-Market Strategy](09-development/go-to-market-strategy.md) - Business and marketing strategy
- [Launch Plan](09-development/launch-plan.md) - 7-day tactical execution plan

## Packaging & Release
- [Release Process](09-packaging/release-process.md) - How to release
- [Packaging Guide](09-packaging/README.md) - Building packages
- [Release Quickstart](09-packaging/RELEASE_QUICKSTART.md) - Quick release guide

## By Use Case

### I want to...
- **Get started quickly** → [Getting Started](01-getting-started/README.md)
- **Integrate LGX into my project** → [Integration Guide](04-integration/lgx_runtime_integration_guide.md)
- **Understand the API** → [API Reference](02-api-reference/README.md)
- **Learn the architecture** → [System Architecture](03-architecture/lgx_runtime_architecture.md)
- **See performance data** → [Performance Results](05-performance/STK_LGX_REAL_PERFORMANCE.md)
- **Run tests** → [Test Suite](06-testing/complete-suite.md)
- **Deploy to production** → [Operations Guide](08-operations/README.md)
- **Contribute code** → [Development Docs](09-development/README.md)
- **Build packages** → [Packaging Guide](09-packaging/README.md)

## By Role

### Game Developer
1. [Getting Started](01-getting-started/README.md)
2. [Runtime API](02-api-reference/lgx_runtime_api.md)
3. [Integration Guide](04-integration/lgx_runtime_integration_guide.md)
4. [Performance Targets](05-performance/targets.md)

### System Administrator
1. [Hardware Compatibility](08-operations/hardware-compatibility.md)
2. [Graceful Degradation](08-operations/graceful-degradation.md)
3. [Telemetry Privacy](08-operations/telemetry-privacy.md)

### Performance Engineer
1. [System Architecture](03-architecture/lgx_runtime_architecture.md)
2. [Performance Results](05-performance/STK_LGX_REAL_PERFORMANCE.md)
3. [Benchmarks](05-performance/benchmarks.md)
4. [Optimization Report](05-performance/optimization-report.md)

### Security Auditor
1. [Security Overview](07-security/README.md)
2. [Threat Model](07-security/threat-model.md)
3. [Security Testing](07-security/testing.md)
4. [Audit Preparation](07-security/audit-preparation.md)

### Contributor
1. [Project History](09-development/project-history.md)
2. [Implementation Plan](09-development/implementation_plan.md)
3. [Current Tasks](09-development/task.md)
4. [Test Suite](06-testing/complete-suite.md)

## Documentation Status

**Complete Sections**
- API Reference (all modules documented)
- Integration Guide (with real-world examples)
- Performance Documentation (with verified results)
- Testing Documentation (complete suite)
- Security Documentation (audit-ready)
- Operations Guide (deployment-ready)
- Packaging Documentation (all formats)

**In Progress**
- Additional code examples
- Video tutorials
- Interactive guides

## Contributing to Documentation

See [CONTRIBUTING.md](../CONTRIBUTING.md) for documentation contribution guidelines.

---

**Last Updated**: 2026-03-18  
**Documentation Version**: 1.0.1
