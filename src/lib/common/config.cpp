/* A configuration class for the database engine. Pur*/
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

  // Conservative defaults for embedded use
  config.buffer_pool_size = 1024;                   // 4 MB (1024 pages × 4 KB)
  config.block_cache_size_bytes = 64 * 1024 * 1024; // 64 MB (smaller for embedded)
  config.wal_sync_mode = WalSyncMode::kEveryWrite;  // Safety over speed

  return config;
}

DatabaseConfig DatabaseConfig::Production(std::filesystem::path root_path) {
  DatabaseConfig config;

  config.root_dir = std::move(root_path);

  // Separate directories for data and WAL (can be different volumes)
  config.data_dir = config.root_dir / "data";
  config.wal_dir = config.root_dir / "wal";

  // Production-grade settings
  config.buffer_pool_size = 16384;                   // 64 MB (16384 pages × 4 KB)
  config.block_cache_size_bytes = 512 * 1024 * 1024; // 512 MB
  config.wal_sync_mode = WalSyncMode::kEveryWrite;

  return config;
}

DatabaseConfig DatabaseConfig::Development(std::filesystem::path db_path) {
  DatabaseConfig config;

  // Similar to embedded, but with relaxed durability for speed
  config.root_dir = std::move(db_path);
  config.data_dir = config.root_dir;
  config.wal_dir = config.root_dir;

  // Fast settings for development
  config.buffer_pool_size = 1024; // 4 MB
  config.block_cache_size_bytes = 128 * 1024 * 1024;
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

std::filesystem::path DatabaseConfig::GetPageDataPath(uint64_t page_id) const {
  // Page-oriented architecture: flat structure, all pages in data_dir
  return data_dir / ("page_" + std::to_string(page_id) + ".dat");
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

  // Page-oriented architecture uses flat structure - no level directories needed

  return true;
}

} // namespace core_engine
