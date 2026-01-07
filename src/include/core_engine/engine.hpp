#pragma once

// core_engine/engine.hpp
//
// Purpose:
// - Small, stable façade for embedding the engine.
// - Keeps subsystems behind one well-known lifecycle entry point.
//
// Current state (Year 1 Q4 - Write-Ahead Logging):
// - Open creates DiskManager for page-level I/O.
// - BufferPoolManager caches pages with LRU-K eviction (Q3).
// - LogManager provides WAL for durability and recovery (Q4).
// - Put/Get use buffered page storage with WAL logging.
// - Execute is reserved for a future SQL/query layer.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <core_engine/common/config.hpp>
#include <core_engine/common/status.hpp>
#include <core_engine/storage/buffer_pool_manager.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/log_manager.hpp>
#include <core_engine/vector/hnsw_index.hpp>
#include <core_engine/vector/vector.hpp>

namespace core_engine {

// Forward declarations
namespace vector {
class HNSWIndex;
}

// ScanOptions: Configuration for range queries (v1.4)
// Moved outside Engine class to avoid forward reference issues with default parameters
struct ScanOptions {
  bool reverse = false;           // Scan in descending order.
  std::size_t limit = 0;          // Maximum results (0 = unlimited).
  bool keys_only = false;         // Return keys only (no values).
};

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
  ~Engine();

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
  std::vector<std::pair<std::string, std::string>> 
    Scan(const std::string& start_key, const std::string& end_key, const ScanOptions& options = ScanOptions());

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

  // ====== Group Commit Optimization (v1.5) ======

  // Begin a batch of writes. Multiple Put/Delete operations will share the same
  // transaction and delay fsync until EndBatch() or Flush() is called.
  // This dramatically improves bulk write performance by reducing disk sync overhead.
  //
  // Example:
  //   engine.BeginBatch();
  //   for (int i = 0; i < 10000; i++) {
  //     engine.Put("key" + std::to_string(i), "value");
  //   }
  //   engine.EndBatch();  // Single fsync for all 10000 writes
  void BeginBatch();

  // End the current batch and flush all buffered writes to disk.
  // Returns Status indicating whether the batch was successfully committed.
  Status EndBatch();

  // Explicitly flush all buffered writes to disk without ending the batch.
  // Useful for checkpointing during long-running batch operations.
  Status Flush();

  // Check if currently in batch mode.
  bool InBatchMode() const {
    return batch_mode_;
  }

 private:
   std::unique_ptr<DiskManager> disk_manager_;              // Page-level I/O (Year 1 Q1)
   std::unique_ptr<BufferPoolManager> buffer_pool_manager_; // Page cache with LRU-K (Year 1 Q3)
   std::unique_ptr<LogManager> log_manager_;                // Write-Ahead Log (Year 1 Q4)
   bool is_open_ = false;
   TxnId next_txn_id_ = 1; // Transaction ID counter for WAL

   // In-memory index: key -> page_id mapping (prevents O(N) linear scans)
   std::unordered_map<std::string, PageId> key_to_page_;

   // Group commit optimization
   bool batch_mode_ = false; // Whether we're in batch mode
   TxnId batch_txn_id_ = 0;  // Shared transaction ID for current batch
   LSN batch_begin_lsn_ = 0; // Begin LSN for current batch transaction

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
