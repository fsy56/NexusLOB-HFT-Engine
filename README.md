# NexusLOB: Low Latency C++20 Multi-Asset Trading Engine

NexusLOB is a cross-platform, low-latency multi-asset trading and simulation framework engineered in Modern C++ (C++20). Originally developed for Windows environments, it features a native POSIX memory-mapped pipeline optimized specifically for Linux production architectures.

## 📊 Live Verification Performance Matrix

| Metric Signature | Linux Production Profile (WSL/GCC 13.3) | Windows Baseline Profile (MSVC) |
| :--- | :--- | :--- |
| **Total Multi-Asset Ticks** | 7,500,000 records | 8,144,274 records |
| **Peak System Throughput** | **69.23 Million TPS** | 31.89 Million TPS |
| **Deterministic Core Path Latency** | **14.48 nanoseconds/path** | 32.14 nanoseconds/path |
| **Microarchitectural Hardware Budget** | **50.7 CPU clock cycles/tick** | 112.5 CPU clock cycles/tick |
| **SLA Performance Status** | **PASSED (Sub-15ns Deterministic Matrix)** | PASSED (Sub-100ns Deterministic Matrix) |

*Note: Linux performance values represent hardware performance metrics captured via native performance counter telemetry (`perf stat`) with optimization models (`-O3 -march=native -flto`) fully engaged.*
