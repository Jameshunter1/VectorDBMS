#pragma once

// core_engine/engine.hpp
//
// Purpose:
// - Small, stable façade for embedding the engine.
// - Keeps subsystems behind one well-known lifecycle entry point.
//
// Current state (Year 1 Q2 - Buffer Pool Layer):
// - Open creates DiskManager for page-level I/O.
// - BufferPoolManager caches pages with LRU eviction.
// - Put/Get use buffered page storage with pin/unpin semantics.
// - Execute is reserved for a future SQL/query layer.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <core_engine/common/config.hpp>
#include <core_engine/common/status.hpp>
#include <core_engine/storage/buffer_pool_manager.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/vector/hnsw_index.hpp>
#include <core_engine/vector/vector.hpp>

namespace core_engine {

// Forward declarations
namespace vector {
class HNSWIndex;
}

// Engine is the façade your application would embed.
//
// Intent:
// - Keep it small and stable: other subsystems can evolve behind it.
// - Prefer explicit lifecycle: Open/Close make ownership and resource
//   boundaries easy to reason about.
//
// This starter implementation wires a few subsystems together without trying
// to be a real database yet.
class Engine {
 public:
  Engine();

  // Opens (or creates) a database at the given path (embedded mode).
  Status Open(std::filesystem::path db_path);
  
  // Opens with explicit configuration (production mode).
  Status Open(const DatabaseConfig& config);

  // Year 1 Q2 page-based API with buffer pool.
  //
  // These methods use BufferPoolManager for efficient caching:
  // - Put writes to pages via BufferPoolManager (cached).
  // - Get reads from pages via BufferPoolManager (cached).
  // - Delete removes keys (future: tombstone pages).
  Status Put(std::string key, std::string value);
  std::optional<std::string> Get(std::string key);
  Status Delete(std::string key);
  
  // v1.4: Advanced batch operations for improved performance.
  // Batch operations reduce WAL sync overhead by grouping writes.
  struct BatchOperation {
    enum class Type { PUT, DELETE };
    Type type;
    std::string key;
    std::string value;  // Empty for DELETE operations.
  };
  Status BatchWrite(const std::vector<BatchOperation>& operations);
  std::vector<std::optional<std::string>> BatchGet(const std::vector<std::string>& keys);
  
  // v1.4: Range query support for scanning key ranges.
  // Returns all key-value pairs where start_key <= key < end_key.
  // If reverse=true, returns results in descending order.
  // Limit controls maximum number of results (0 = unlimited).
  struct ScanOptions {
    bool reverse = false;           // Scan in descending order.
    std::size_t limit = 0;          // Maximum results (0 = unlimited).
    bool keys_only = false;         // Return keys only (no values).
  };
  std::vector<std::pair<std::string, std::string>> 
    Scan(const std::string& start_key, const std::string& end_key, const ScanOptions& options = {});

  // ====== Vector Database Operations (v2.0) ======

  // Insert or update a vector with associated key.
  // The vector is stored via page I/O and indexed in HNSW for fast similarity search.
  Status PutVector(const std::string& key, const vector::Vector& vec);
  
  // Search for k most similar vectors to the query.
  // Returns results sorted by distance (ascending = more similar).
  struct VectorSearchResult {
    std::string key;             // Key of the similar vector
    float distance;              // Distance score (lower = more similar)
    vector::Vector vector;       // The actual vector (optional, set by include_vectors)
  };
  std::vector<VectorSearchResult> SearchSimilar(const vector::Vector& query, std::size_t k, bool include_vectors = false);
  
  // Retrieve a vector by key
  std::optional<vector::Vector> GetVector(const std::string& key);
  
  // Batch vector operations for improved performance
  Status BatchPutVectors(const std::vector<std::pair<std::string, vector::Vector>>& vectors);
  std::vector<std::optional<vector::Vector>> BatchGetVectors(const std::vector<std::string>& keys);
  
  // Get vector index statistics
  struct VectorStats {
    bool index_enabled = false;         // Whether vector indexing is active
    std::size_t num_vectors = 0;        // Total vectors indexed
    std::size_t dimension = 0;          // Vector dimension
    std::string metric;                 // Distance metric name
    std::size_t num_layers = 0;         // HNSW layers
    double avg_connections_per_node = 0.0; // HNSW graph density
  };
  VectorStats GetVectorStats() const;

  // Executes a statement (placeholder). Eventually this takes a parsed AST
  // or physical plan rather than raw SQL text.
  Status Execute(std::string_view statement);

  // Get database statistics for monitoring/debugging.
  struct Stats {
    std::size_t total_pages;       // Total pages in database file.
    std::size_t total_reads;       // Total page reads from disk.
    std::size_t total_writes;      // Total page writes to disk.
    std::size_t checksum_failures; // Corrupted pages detected.

    // Performance metrics (microseconds)
    double avg_get_time_us = 0.0;       // Average Get operation time.
    double avg_put_time_us = 0.0;       // Average Put operation time.
    std::size_t total_gets = 0;         // Total Get operations performed.
    std::size_t total_puts = 0;         // Total Put operations performed.
  };
  Stats GetStats() const;

  // Get all entries (for viewing).
  std::vector<std::pair<std::string, std::string>> GetAllEntries() const;

 private:
   std::unique_ptr<DiskManager> disk_manager_;              // Page-level I/O (Year 1 Q1)
   std::unique_ptr<BufferPoolManager> buffer_pool_manager_; // Page cache with LRU (Year 1 Q2)
   bool is_open_ = false;

   // Vector database components
   std::unique_ptr<vector::HNSWIndex> vector_index_; // HNSW index for similarity search
   DatabaseConfig config_;                           // Store config for vector settings

   // Performance tracking
   mutable std::size_t total_gets_ = 0;
   mutable std::size_t total_puts_ = 0;
   mutable double total_get_time_us_ = 0.0;
   mutable double total_put_time_us_ = 0.0;
};

}  // namespace core_engine
