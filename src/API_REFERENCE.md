# VectorDB API Reference

## Core API

### Engine Class

The main entry point for all database operations.

```cpp
#include <core_engine/engine.hpp>

core_engine::Engine engine;
```

### Configuration

```cpp
// Embedded mode (single directory)
DatabaseConfig config = DatabaseConfig::Embedded("./my_db");

// Production mode (separate data/WAL)
DatabaseConfig config = DatabaseConfig::Production("/var/lib/vectordb");

// Vector settings
config.enable_vector_index = true;
config.vector_dimension = 128;          // Must match your vectors
config.vector_metric = DatabaseConfig::VectorDistanceMetric::kCosine;

// HNSW tuning
config.hnsw_params.M = 16;              // Higher = better recall, more memory
config.hnsw_params.ef_construction = 200; // Higher = better index quality
config.hnsw_params.ef_search = 50;      // Higher = better recall, slower search

engine.Open(config);
```

### Vector Operations

#### Insert Vector
```cpp
vector::Vector vec({0.1f, 0.2f, 0.3f, ...});  // Your embedding
Status s = engine.PutVector("doc1", vec);
```

#### Search Similar Vectors
```cpp
vector::Vector query({0.15f, 0.25f, ...});
auto results = engine.SearchSimilar(query, /*k=*/10);

for (const auto& result : results) {
    std::cout << result.key << ": " << result.distance << "\n";
}
```

#### Get Vector by Key
```cpp
auto vec = engine.GetVector("doc1");
if (vec) {
    // Use vector
}
```

#### Batch Operations (10x faster)
```cpp
// Batch insert
std::vector<std::pair<std::string, vector::Vector>> batch = {
    {"doc1", vec1}, {"doc2", vec2}, {"doc3", vec3}
};
engine.BatchPutVectors(batch);

// Batch get
std::vector<std::string> keys = {"doc1", "doc2", "doc3"};
auto vectors = engine.BatchGetVectors(keys);
```

## Distance Metrics

```cpp
// Cosine Similarity (default for text/semantic search)
config.vector_metric = DatabaseConfig::VectorDistanceMetric::kCosine;

// Euclidean Distance (L2, good for spatial data)
config.vector_metric = DatabaseConfig::VectorDistanceMetric::kEuclidean;

// Dot Product (for maximum inner product search)
config.vector_metric = DatabaseConfig::VectorDistanceMetric::kDotProduct;

// Manhattan Distance (L1, robust to outliers)
config.vector_metric = DatabaseConfig::VectorDistanceMetric::kManhattan;
```

## Statistics & Monitoring

```cpp
// Engine stats
auto stats = engine.GetStats();
std::cout << "MemTable size: " << stats.memtable_size_bytes << "\n";
std::cout << "Total vectors: " << stats.memtable_entry_count << "\n";

// Vector index stats
auto vec_stats = engine.GetVectorStats();
std::cout << "Vectors indexed: " << vec_stats.num_vectors << "\n";
std::cout << "HNSW layers: " << vec_stats.num_layers << "\n";
std::cout << "Avg connections: " << vec_stats.avg_connections_per_node << "\n";
```

## Performance Tuning

### HNSW Parameters

| Parameter | Default | Description | Trade-off |
|-----------|---------|-------------|-----------|
| `M` | 16 | Max connections per node | Higher = better recall, more memory |
| `ef_construction` | 200 | Build-time search depth | Higher = better quality, slower inserts |
| `ef_search` | 50 | Query-time search depth | Higher = better recall, slower queries |

### Typical Settings

**High Accuracy** (RAG systems, production search):
```cpp
config.hnsw_params.M = 32;
config.hnsw_params.ef_construction = 400;
config.hnsw_params.ef_search = 100;
```

**Balanced** (default):
```cpp
config.hnsw_params.M = 16;
config.hnsw_params.ef_construction = 200;
config.hnsw_params.ef_search = 50;
```

**High Speed** (real-time recommendations):
```cpp
config.hnsw_params.M = 8;
config.hnsw_params.ef_construction = 100;
config.hnsw_params.ef_search = 20;
```

## Error Handling

```cpp
Status s = engine.PutVector("key", vec);
if (!s.ok()) {
    std::cerr << "Error: " << s.ToString() << "\n";
    
    if (s.code() == StatusCode::kInvalidArgument) {
        // Handle validation error
    } else if (s.code() == StatusCode::kNotFound) {
        // Handle missing key
    }
}
```

## Thread Safety

All operations are thread-safe:
- Concurrent reads are fully parallelized
- Writes use exclusive locks
- Search operations don't block each other

```cpp
// Safe to call from multiple threads
std::thread t1([&]() { engine.PutVector("k1", v1); });
std::thread t2([&]() { engine.SearchSimilar(query, 10); });
std::thread t3([&]() { auto v = engine.GetVector("k2"); });
```

## Best Practices

1. **Use batch operations** for bulk inserts (10x faster)
2. **Normalize vectors** for cosine similarity (or use built-in normalization)
3. **Tune ef_search** based on recall requirements
4. **Monitor memory** - each vector uses ~M×layers connections
5. **Use production mode** for separate WAL/data directories
6. **Enable authentication** in production environments

## Examples

### Semantic Search
```cpp
// Store document embeddings
for (const auto& [doc_id, embedding] : documents) {
    engine.PutVector(doc_id, embedding);
}

// Search for similar documents
auto results = engine.SearchSimilar(query_embedding, 10);
```

### Image Similarity
```cpp
// Store image feature vectors
engine.PutVector("image123.jpg", image_features);

// Find similar images
auto similar = engine.SearchSimilar(query_image_features, 5);
```

### Recommendation System
```cpp
// Store user/item embeddings
engine.BatchPutVectors(item_embeddings);

// Find recommended items for user
auto recommendations = engine.SearchSimilar(user_embedding, 20);
```
