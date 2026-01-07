#include <core_engine/common/config.hpp>
#include <filesystem>
#include <system_error>

namespace core_engine {

// ====== Factory Methods ======

DatabaseConfig DatabaseConfig::Embedded(std::filesystem::path db_path) {
  DatabaseConfig config;

  // All files in the same directory (simple, portable)
  config.root_dir = std::move(db_path);
  config.data_dir = config.root_dir;
  config.wal_dir = config.root_dir;

  // Use level directories for better organization
  config.use_level_directories = true;

  // Conservative defaults for embedded use
  config.buffer_pool_size = 1024;                   // 4 MB (1024 pages × 4 KB)
  config.block_cache_size_bytes = 64 * 1024 * 1024; // 64 MB (smaller for embedded)
  config.l0_compaction_trigger = 4;
  config.wal_sync_mode = WalSyncMode::kEveryWrite; // Safety over speed

  return config;
}

DatabaseConfig DatabaseConfig::Production(std::filesystem::path root_path) {
  DatabaseConfig config;

  config.root_dir = std::move(root_path);

  // Separate directories for data and WAL (can be different volumes)
  config.data_dir = config.root_dir / "data";
  config.wal_dir = config.root_dir / "wal";

  // Use level directories for production scalability
  config.use_level_directories = true;

  // Production-grade settings
  config.buffer_pool_size = 16384;                   // 64 MB (16384 pages × 4 KB)
  config.block_cache_size_bytes = 512 * 1024 * 1024; // 512 MB
  config.l0_compaction_trigger = 4;
  config.wal_sync_mode = WalSyncMode::kEveryWrite;

  return config;
}

DatabaseConfig DatabaseConfig::Development(std::filesystem::path db_path) {
  DatabaseConfig config;

  // Similar to embedded, but with relaxed durability for speed
  config.root_dir = std::move(db_path);
  config.data_dir = config.root_dir;
  config.wal_dir = config.root_dir;

  config.use_level_directories = true;

  // Fast settings for development
  config.buffer_pool_size = 1024; // 4 MB
  config.block_cache_size_bytes = 128 * 1024 * 1024;
  config.l0_compaction_trigger = 4;
  config.wal_sync_mode = WalSyncMode::kNone; // Faster, but data loss on crash is acceptable in dev

  return config;
}

DatabaseConfig DatabaseConfig::LoadFromFile(const std::filesystem::path& config_file) {
  // TODO: Implement YAML/JSON parsing
  // For now, just return a default embedded config
  (void)config_file;
  return Embedded("./default_db");
}

// ====== Path Helpers ======

std::filesystem::path DatabaseConfig::GetLevelPath(int level) const {
  if (!use_level_directories) {
    // Legacy flat structure: all page data in data_dir
    return data_dir;
  }

  // Modern structure: data_dir/level_0/, data_dir/level_1/, etc.
  return data_dir / ("level_" + std::to_string(level));
}

std::filesystem::path DatabaseConfig::GetSSTablePath(uint64_t sstable_id, int level) const {
  std::filesystem::path level_dir = GetLevelPath(level);
  return level_dir / ("page_" + std::to_string(sstable_id) + ".dat");
}

std::filesystem::path DatabaseConfig::GetWalPath() const {
  return wal_dir / "wal.log";
}

std::filesystem::path DatabaseConfig::GetManifestPath() const {
  // Manifest stays in data directory (loaded on startup with pages)
  return data_dir / "MANIFEST";
}

bool DatabaseConfig::Initialize() const {
  std::error_code ec;

  // Create root directory
  std::filesystem::create_directories(root_dir, ec);
  if (ec) {
    return false;
  }

  // Create data directory
  std::filesystem::create_directories(data_dir, ec);
  if (ec) {
    return false;
  }

  // Create WAL directory
  std::filesystem::create_directories(wal_dir, ec);
  if (ec) {
    return false;
  }

  // If using level directories, create level_0 upfront
  // (other levels created on demand during compaction)
  if (use_level_directories) {
    std::filesystem::create_directories(GetLevelPath(0), ec);
    if (ec) {
      return false;
    }
  }

  return true;
}

} // namespace core_engine
