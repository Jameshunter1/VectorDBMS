#pragma once

// MemTable for an LSM-first engine.
//
// MemTable is the in-memory, mutable structure that buffers recent writes.
// It is periodically flushed to disk as an immutable SSTable.

#include <map>       // std::map keeps keys sorted; useful for range flush.
#include <mutex>     // std::mutex for coarse thread-safety (starter).
#include <optional>  // std::optional for Get.
#include <string>    // std::string.
#include <vector>    // std::vector for GetAllSorted.

namespace core_engine {

// MemTable is a minimal, thread-safe in-memory KV map.
//
// Why std::map?
// - Keeps keys ordered, making future flush to SSTable easier.
// - Not the fastest, but extremely simple and correct.
//
// Tombstones:
// - Deleted keys are marked with a special tombstone value
// - During Get(), tombstones return nullopt (key not found)
// - During flush, tombstones are written to SSTables to shadow older values
//
// Later upgrades:
// - skiplist
// - lock-free structures
// - arena allocation
class MemTable {
 public:
  MemTable() = default;

  // Inserts or overwrites.
  void Put(std::string key, std::string value);

  // Marks a key as deleted (tombstone).
  void Delete(std::string key);

  // Returns value if present and not deleted.
  // Returns nullopt if key is missing or tombstone.
  std::optional<std::string> Get(const std::string& key) const;

  // Returns number of keys (including tombstones).
  std::size_t Size() const;

  // Returns approximate memory usage (key + value bytes).
  std::size_t ApproximateSizeBytes() const;

  // Returns all entries in sorted order (for flushing to SSTable).
  // Includes tombstones (marked with kTombstoneValue).
  std::vector<std::pair<std::string, std::string>> GetAllSorted() const;

  // Clears all entries (used after flush).
  void Clear();

  // Special marker value for tombstones.
  // We use a string that's unlikely to be a real value.
  static inline const std::string kTombstoneValue = "\x00__TOMBSTONE__\x00";

 private:
  mutable std::mutex mutex_{};                // Protects map_ for starter thread-safety.
  std::map<std::string, std::string> map_{};  // Sorted key/value map (values can be tombstones).
  std::size_t approx_size_bytes_ = 0;         // Approximate memory usage (sum of key+value sizes).
};

}  // namespace core_engine
