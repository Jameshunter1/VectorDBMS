#pragma once

// LSMTree is the storage engine core (starter).
//
// Real LSM engines implement:
// - MemTable + immutable MemTable(s)
// - WAL + recovery
// - SSTables + block cache + bloom filters
// - compaction + manifest
//
// This starter keeps only: MemTable + WAL append, to make the project tangible.

#include <filesystem>  // std::filesystem::path.
#include <optional>    // std::optional.
#include <string>      // std::string.

#include <core_engine/common/config.hpp>       // core_engine::DatabaseConfig.
#include <core_engine/common/status.hpp>       // core_engine::Status.
#include <core_engine/kv/key_value.hpp>        // core_engine::KeyValueStore.
#include <core_engine/lsm/level.hpp>           // core_engine::LeveledLSM.
#include <core_engine/lsm/manifest.hpp>        // core_engine::Manifest.
#include <core_engine/lsm/memtable.hpp>        // core_engine::MemTable.
#include <core_engine/lsm/sstable.hpp>         // core_engine::SSTableReader.
#include <core_engine/lsm/wal.hpp>             // core_engine::Wal.

#include <memory>       // std::unique_ptr.
#include <vector>       // std::vector.

namespace core_engine {

class LsmTree final : public KeyValueStore {
 public:
  // Construct without opening any files.
  LsmTree();

  // Open/create the storage directory and WAL (legacy API - uses embedded config).
  Status Open(std::filesystem::path db_dir);
  
  // Open with explicit configuration (production-ready API).
  Status Open(const DatabaseConfig& config);

  // KeyValueStore API.
  Status Put(std::string key, std::string value) override;
  std::optional<std::string> Get(std::string key) override;
  Status Delete(std::string key) override;

  // Statistics introspection.
  std::size_t GetMemTableSizeBytes() const;
  std::size_t GetMemTableEntryCount() const;
  std::size_t GetSSTableCount() const;
  
  // Bloom filter statistics (aggregated across all SSTables).
  std::size_t GetBloomFilterChecks() const;
  std::size_t GetBloomFilterHits() const;
  std::size_t GetBloomFilterFalsePositives() const;

  // Get all entries (for viewing/debugging).
  std::vector<std::pair<std::string, std::string>> GetAllEntries() const;

 private:
  Status MaybeFlushMemTable();                 // Flush MemTable to SSTable if size threshold exceeded.
  Status FlushMemTable();                      // Flush current MemTable to a new SSTable.
  Status RecoverFromManifest();                // Load SSTables listed in the manifest on startup.
  
  // Get the appropriate directory for an SSTable based on its level.
  std::filesystem::path GetSSTablePath(uint64_t sstable_id, int level) const;

  DatabaseConfig config_{};                    // Configuration (paths, thresholds, etc.).
  std::filesystem::path db_dir_{};             // Root directory (legacy, kept for compatibility).
  Wal wal_{std::filesystem::path{}};           // WAL writer.
  MemTable memtable_{};                        // In-memory write buffer.
  Manifest manifest_{};                        // Tracks which SSTables are active.
  LeveledLSM leveled_lsm_{};                   // Multi-level LSM structure for efficient compaction.
  std::uint64_t next_sstable_id_ = 0;          // Counter for unique SSTable file names.
  bool is_open_ = false;                       // Prevent use before Open.

  static constexpr std::size_t kMemTableFlushThresholdBytes = 4 * 1024 * 1024; // 4 MB flush threshold.
};

}  // namespace core_engine
