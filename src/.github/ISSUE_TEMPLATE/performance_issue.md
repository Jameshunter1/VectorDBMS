---
name: Performance Issue
about: Report a performance problem or suggest an optimization
title: '[PERF] '
labels: performance
assignees: ''
---

## Performance Issue Description
A clear and concise description of the performance problem.

## Environment
- **OS**: [e.g., Windows 11, Ubuntu 22.04]
- **Compiler**: [e.g., MSVC 2022, GCC 11.3]
- **Build Type**: [Debug/Release/RelWithDebInfo]
- **CPU**: [e.g., Intel i7-12700K, AMD Ryzen 9 5900X]
- **RAM**: [e.g., 32GB]
- **Storage**: [e.g., NVMe SSD, HDD]

## Benchmark Results
Provide performance metrics:

### Current Performance
```
Operation: [e.g., bulk insert]
Dataset Size: [e.g., 1 million keys]
Time: [e.g., 45 seconds]
Throughput: [e.g., 22,000 ops/sec]
Memory Usage: [e.g., 2.5GB]
CPU Usage: [e.g., 85%]
```

### Expected Performance
```
Time: [e.g., 15 seconds]
Throughput: [e.g., 66,000 ops/sec]
```

## Reproducible Test Case
```cpp
// Code to reproduce the performance issue
```

## Profiling Data
If you've profiled the code, provide:
- Hot spots / bottlenecks
- CPU profile data
- Memory allocation patterns
- I/O statistics

## Suggested Optimization
(Optional) If you have ideas for improvement:
- Algorithm changes
- Data structure optimizations
- Caching strategies
- Parallelization opportunities

## Comparison with Similar Systems
How does the performance compare to similar databases or operations?

## Additional Context
- Is this a regression from a previous version?
- Does it only occur with specific data patterns?
- Any relevant configuration settings?
