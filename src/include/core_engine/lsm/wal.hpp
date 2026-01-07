#pragma once

// Write-Ahead Log (WAL) for an LSM-first engine.
//
// In an LSM, durability typically comes from:
// - appending updates to a WAL (sequential I/O)
// - keeping the latest state in a MemTable
// - flushing MemTable into immutable SSTables
//
// This file implements only the *append* part for now.

#include <cstdint>     // fixed-width integer types.
#include <filesystem>  // std::filesystem::path.
#include <functional>  // std::function for replay callback.
#include <string>      // std::string.

#include <core_engine/common/status.hpp>  // core_engine::Status.

namespace core_engine {

// WalRecordType is kept explicit to future-proof backward/forward compatibility.
enum class WalRecordType : std::uint8_t {
  kPut = 1,   // Record represents a Put(key,value).
  kDelete = 2 // Record represents a Delete(key).
};

// Wal is an append-only log.
//
// File format (starter, not final):
// [1 byte type][4 bytes key_len][4 bytes value_len][key bytes][value bytes]
//
// Notes:
// - No checksums yet.
// - No fsync policy yet.
// - No recovery reader yet.
class Wal {
 public:
  // Creates a WAL writer bound to a path.
  explicit Wal(std::filesystem::path path);

  // Opens or creates the WAL file.
  Status OpenOrCreate();

  // Appends a Put record.
  Status AppendPut(const std::string& key, const std::string& value);

  // Appends a Delete record (tombstone).
  // A tombstone marks a key as deleted without actually removing it yet. This is nee
  Status AppendDelete(const std::string& key);

  // Replays the WAL from the beginning.
  //
  // This is the durability milestone that makes values survive restarts.
  // The callback is called for each record in order.
  using ReplayCallback = std::function<Status(WalRecordType type, std::string key, std::string value)>;
  Status Replay(const ReplayCallback& apply);

 private:
  std::filesystem::path path_; // WAL file location on disk.
};

}  // namespace core_engine
