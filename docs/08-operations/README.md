# Operations Documentation

Operational guides for deployment, monitoring, and troubleshooting.

## Contents

### Operational Guides
- [telemetry-privacy.md](telemetry-privacy.md) - Telemetry privacy guide
- [telemetry-verification.md](telemetry-verification.md) - SHA-256 verification for telemetry
- [hardware-compatibility.md](hardware-compatibility.md) - Hardware compatibility matrix
- [graceful-degradation.md](graceful-degradation.md) - Graceful degradation guide

### Video Recording & Documentation
- [VIDEO_RECORDING_GUIDE.md](VIDEO_RECORDING_GUIDE.md) - Guide for recording demos and presentations
- [RECORDING_SUMMARY.md](RECORDING_SUMMARY.md) - Summary of recorded content
- [READY_FOR_VIDEO.md](READY_FOR_VIDEO.md) - Video recording readiness checklist

## Telemetry

The LGX Runtime includes optional telemetry for performance monitoring:

- **Privacy-first**: Opt-in, transparent, user-controlled
- **Anonymized**: SHA-256 hashing for sensitive data
- **Separate process**: Isolated from game process
- **Minimal overhead**: < 1% performance impact

See [telemetry-privacy.md](telemetry-privacy.md) for details.

## Hardware Compatibility

The runtime supports diverse hardware configurations:

- **Tier 1 (Optimal)**: Modern GPU, huge pages, NUMA
- **Tier 2 (Compatible)**: Standard hardware
- **Tier 3 (Degraded)**: Limited hardware with software fallbacks

See [hardware-compatibility.md](hardware-compatibility.md) for the complete matrix.

## Graceful Degradation

The runtime gracefully degrades when hardware features are unavailable:

- **No GPU**: Falls back to system memory
- **No huge pages**: Uses standard pages
- **No NUMA**: Uses single-node allocation

See [graceful-degradation.md](graceful-degradation.md) for details.
