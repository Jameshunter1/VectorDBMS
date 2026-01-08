#pragma once

// core_engine/vector/hnsw_index.hpp
//
// Purpose:
// - Hierarchical Navigable Small World (HNSW) graph for fast approximate nearest neighbor search
// - Industry-standard algorithm used by pgvector, Faiss, Qdrant, Milvus
// - Provides O(log N) search complexity with high recall
//
// Key Concepts:
// - Multi-layer graph structure (layers 0 to top layer)
// - Layer 0 contains all vectors, higher layers have exponentially fewer nodes
// - Each node connects to M neighbors at each layer (proximity graph)
// - Search starts from top layer and descends, getting progressively more accurate
//
// Performance Characteristics:
// - Insert: O(log N) with graph updates
// - Search: O(log N) approximate nearest neighbors
// - Memory: ~(M * layers) connections per vector
// - Typical params: M=16, ef_construction=200, ef_search=50

#include <core_engine/common/status.hpp>
#include <core_engine/vector/vector.hpp>

#include <optional>
#include <random>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace core_engine {
namespace vector {

// HNSWIndex implements the HNSW algorithm for approximate nearest neighbor search.
class HNSWIndex {
public:
  // Construction parameters
  struct Params {
    std::size_t dimension = 0;                       // Vector dimension (must match all vectors)
    DistanceMetric metric = DistanceMetric::kCosine; // Distance metric
    std::size_t M = 16; // Max connections per node (higher = better recall, more memory)
    std::size_t ef_construction = 200; // Size of dynamic candidate list during construction (higher
                                       // = better quality, slower inserts)
    std::size_t ef_search =
        50; // Size of dynamic candidate list during search (higher = better recall, slower queries)
    std::size_t max_layers = 16; // Maximum number of layers (auto-calculated if 0)
    double level_multiplier = 1.0 / std::log(2.0); // Controls layer distribution
  };

  // Search result: (key, distance)
  struct SearchResult {
    std::string key;
    float distance;

    bool operator<(const SearchResult& other) const {
      return distance < other.distance;
    }
  };

  explicit HNSWIndex(Params params);

  // Insert a vector with associated key
  Status Insert(const std::string& key, const Vector& vec);

  // Search for k nearest neighbors
  std::vector<SearchResult> Search(const Vector& query, std::size_t k) const;

  // Remove a vector (immediate removal from index)
  Status Remove(const std::string& key);

  // Get statistics
  struct Stats {
    std::size_t num_vectors = 0;           // Total vectors in index
    std::size_t num_layers = 0;            // Number of active layers
    std::size_t total_connections = 0;     // Total graph edges
    double avg_connections_per_node = 0.0; // Average degree
  };
  Stats GetStats() const;

  // Enumerate all stored vectors (used for admin tooling)
  std::vector<std::pair<std::string, Vector>> GetAllVectors() const;

  // Serialization (for persisting via BufferPoolManager)
  std::string Serialize() const;
  static HNSWIndex Deserialize(const std::string& data);

  // Get the dimension this index expects
  std::size_t dimension() const {
    return params_.dimension;
  }
  DistanceMetric metric() const {
    return params_.metric;
  }

private:
  // Internal node structure
  struct Node {
    std::string key;                                // User-provided key
    Vector vector;                                  // The embedding vector
    std::vector<std::unordered_set<int>> neighbors; // Neighbors at each layer [layer -> node_ids]
    int layer = 0;                                  // Maximum layer this node appears in
    bool deleted = false;                           // Tombstone for lazy deletion
  };

  // Core HNSW operations
  int InsertNode(const std::string& key, const Vector& vec);
  std::vector<int> SearchLayer(const Vector& query, int entry_point, std::size_t ef,
                               int layer) const;
  void SelectNeighbors(int node_id, const std::vector<int>& candidates, std::size_t M, int layer);
  int GetRandomLevel() const;
  float Distance(const Vector& a, const Vector& b) const;

  // Graph maintenance
  void PruneConnections(int node_id, int layer);

  // Thread-safe access
  mutable std::shared_mutex mutex_; // Allows concurrent reads, exclusive writes

  // Data structures
  Params params_;
  std::vector<Node> nodes_;                          // All nodes (indexed by node_id)
  std::unordered_map<std::string, int> key_to_node_; // Key -> node_id mapping
  int entry_point_ = -1; // Entry point for search (node at highest layer)
  int max_layer_ = -1;   // Current maximum layer in graph

  // Random number generation for layer selection
  mutable std::mt19937 rng_;
};

} // namespace vector
} // namespace core_engine
