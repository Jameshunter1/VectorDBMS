---
name: Vector Performance Issue
about: Report performance concerns with vector operations
title: '[PERFORMANCE] '
labels: performance, vector
assignees: ''
---

## Performance Issue
Describe the performance problem you're experiencing.

## Operation Type
- [ ] Vector Insertion (`PutVector`)
- [ ] Similarity Search (`SearchSimilar`)
- [ ] Batch Vector Operations
- [ ] Index Building
- [ ] Other: ___

## Dataset Characteristics
- **Number of vectors:** 
- **Vector dimension:** 
- **Distance metric:** [Cosine/Euclidean/Dot Product/Manhattan]

## Configuration
```cpp
DatabaseConfig config;
config.vector_dimension = ...;
config.hnsw_params.M = ...;
config.hnsw_params.ef_construction = ...;
config.hnsw_params.ef_search = ...;
```

## Observed Performance
- **Operation time:** 
- **Throughput:** (ops/sec)
- **Memory usage:** 

## Expected Performance
What performance were you expecting?

## Benchmark Results
If you have benchmark data, please share:
```
Paste benchmark output here
```

## System Information
- **CPU:** 
- **RAM:** 
- **Storage:** [SSD/NVMe/HDD]
- **OS:** 

## Profiling Data
If available, attach profiling results or flamegraphs.

## Suggested Optimization
If you have ideas for improving performance, please share them.
