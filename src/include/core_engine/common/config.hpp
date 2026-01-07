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
 * - Page data organized into level subdirectories for manageability
 * - Standard system paths for production deployments
 * - Configuration file support (future: YAML/JSON parsing)
 */
struct DatabaseConfig {
  // ====== Storage Paths ======

  /// Root directory for the database (used in embedded mode)
  std::filesystem::path root_dir;

  /// Directory for page data files
  /// In embedded mode: same as root_dir
  /// In production: separate volume (e.g., /var/lib/vectis/data or C:\ProgramData\Vectis\data)
  std::filesystem::path data_dir;

  /// Directory for write-ahead log
  /// In embedded mode: same as root_dir
  /// In production: fast disk with write endurance (e.g., /var/lib/vectis/wal)
  std::filesystem::path wal_dir;

  /// Deprecated: Page-oriented architecture stores pages in flat structure (no LSM-style levels)
  /// This field is ignored. All page data files stored directly in data_dir/
  bool use_level_directories = false; // Deprecated

  // ====== Performance Tuning ======

  /// Buffer pool size in pages (Year 1 Q2)
  /// Default: 1024 pages = 4 MB
  std::size_t buffer_pool_size = 1024;

  /// Block cache size in bytes (future: caching page blocks)
  std::size_t block_cache_size_bytes = 256 * 1024 * 1024; // 256 MB

  /// Deprecated: Page-oriented architecture doesn't use LSM-style compaction
  std::size_t l0_compaction_trigger = 0; // Deprecated

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

  /// Get the full path for a page data file
  /// Example: GetPageDataPath(42) -> "data_dir/page_42.dat"
  std::filesystem::path GetPageDataPath(uint64_t page_id) const;

  /// Get the full path for the WAL file
  std::filesystem::path GetWalPath() const;

  /// Get the full path for the MANIFEST file
  std::filesystem::path GetManifestPath() const;

  /// Validate configuration and create necessary directories
  bool Initialize() const;
};

} // namespace core_engine
