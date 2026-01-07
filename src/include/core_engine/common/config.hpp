#pragma once

#include <filesystem>
#include <string>

namespace core_engine {

/**
 * DatabaseConfig - Production-Ready Configuration System
 *
 * Supports both embedded (SQLite-style) and server deployment modes.
 *
 * Key Features:
 * - Separate WAL directory for performance (WAL on fast disk, data on capacity disk)
 * - Level-based SSTable organization (level_0/, level_1/, etc.)
 * - Standard system paths for production deployments
 * - Configuration file support (future: YAML/JSON parsing)
 */
struct DatabaseConfig {
  // ====== Storage Paths ======

  /// Root directory for the database (used in embedded mode)
  std::filesystem::path root_dir;

  /// Directory for SSTable storage
  /// In embedded mode: same as root_dir
  /// In production: separate volume (e.g., /var/lib/vectis/data or C:\ProgramData\Vectis\data)
  std::filesystem::path data_dir;

  /// Directory for write-ahead log
  /// In embedded mode: same as root_dir
  /// In production: fast disk with write endurance (e.g., /var/lib/vectis/wal)
  std::filesystem::path wal_dir;

  /// Whether to organize SSTables into level subdirectories
  /// true: data_dir/level_0/, data_dir/level_1/, etc.
  /// false: all SSTables in data_dir/ (legacy flat structure)
  bool use_level_directories = true;

  // ====== Performance Tuning ======

  /// Buffer pool size in pages (Year 1 Q2)
  /// Default: 1024 pages = 4 MB
  std::size_t buffer_pool_size = 1024;

  /// Block cache size in bytes (future: caching page blocks)
  std::size_t block_cache_size_bytes = 256 * 1024 * 1024; // 256 MB

  /// Number of pages at L0 that trigger compaction (future)
  std::size_t l0_compaction_trigger = 4;

  // ====== Durability Options ======

  enum class WalSyncMode {
    kNone,       // No fsync (fast, but data loss possible on crash)
    kEveryWrite, // fsync after every write (slow, maximum durability)
    kPeriodic    // fsync every N ms (balanced)
  };

  WalSyncMode wal_sync_mode = WalSyncMode::kEveryWrite;

  // ====== Vector Database Configuration ======

  /// Enable vector database features (HNSW index, similarity search)
  bool enable_vector_index = false;

  /// Vector dimension (must be consistent across all vectors)
  std::size_t vector_dimension = 128;

  /// Distance metric for similarity search
  enum class VectorDistanceMetric {
    kCosine,     // Cosine similarity (normalized dot product)
    kEuclidean,  // L2 distance
    kDotProduct, // Maximum inner product search
    kManhattan   // L1 distance
  };
  VectorDistanceMetric vector_metric = VectorDistanceMetric::kCosine;

  /// HNSW index parameters
  struct HNSWParams {
    std::size_t M = 16;                // Max connections per node
    std::size_t ef_construction = 200; // Construction-time search depth
    std::size_t ef_search = 50;        // Query-time search depth
  };
  HNSWParams hnsw_params{};

  // ====== Factory Methods ======

  /**
   * Create embedded database config (SQLite-style)
   * All files in a single directory, suitable for:
   * - Desktop applications
   * - Embedded systems
   * - Development/testing
   *
   * Example: DatabaseConfig::Embedded("./my_app_data")
   */
  static DatabaseConfig Embedded(std::filesystem::path db_path);

  /**
   * Create production server config with separate volumes
   * Pages and WAL on different disks for performance
   *
   * Example (Linux): DatabaseConfig::Production("/var/lib/vectis")
   * Example (Windows): DatabaseConfig::Production("C:\\ProgramData\\Vectis")
   */
  static DatabaseConfig Production(std::filesystem::path root_path);

  /**
   * Create development config (current default behavior)
   * Uses relative paths in project directory
   */
  static DatabaseConfig Development(std::filesystem::path db_path);

  /**
   * Load configuration from YAML/JSON file (future enhancement)
   *
   * Example file structure:
   * storage:
   *   root_dir: "/var/lib/vectis"
   *   use_level_directories: true
   * performance:
   *   buffer_pool_size: 1024
   *   block_cache_size_mb: 256
   * durability:
   *   wal_sync_mode: "every_write"
   */
  static DatabaseConfig LoadFromFile(const std::filesystem::path& config_file);

  // ====== Path Helpers ======

  /// Get the full path for a level directory
  /// Example: GetLevelPath(0) -> "data_dir/level_0"
  std::filesystem::path GetLevelPath(int level) const;

  /// Get the full path for a page data file
  /// Example: GetSSTablePath(42, 1) -> "data_dir/level_1/page_42.dat"
  std::filesystem::path GetSSTablePath(uint64_t sstable_id, int level) const;

  /// Get the full path for the WAL file
  std::filesystem::path GetWalPath() const;

  /// Get the full path for the MANIFEST file
  std::filesystem::path GetManifestPath() const;

  /// Validate configuration and create necessary directories
  bool Initialize() const;
};

} // namespace core_engine
