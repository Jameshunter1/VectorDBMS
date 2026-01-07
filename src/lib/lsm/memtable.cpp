#include <core_engine/lsm/memtable.hpp>

// Every line is commented (as requested) for the new LSM module.

namespace core_engine {

void MemTable::Put(std::string key, std::string value) {         // Insert/overwrite a key.
  std::lock_guard<std::mutex> lock(mutex_);                      // Lock to protect shared state.
  
  auto it = map_.find(key);                                      // Check if key exists.
  if (it != map_.end()) {                                        // If updating existing key...
    approx_size_bytes_ -= it->second.size();                     // ...subtract old value size only.
    approx_size_bytes_ += value.size();                          // ...add new value size.
  } else {                                                       // If inserting new key...
    approx_size_bytes_ += key.size() + value.size();             // ...add both key and value size.
  }                                                              // End size tracking.
  
  map_[std::move(key)] = std::move(value);                       // Store the value.
}                                                                // End Put.

void MemTable::Delete(std::string key) {                         // Mark a key as deleted (tombstone).
  /**
   * TOMBSTONE IMPLEMENTATION
   * 
   * We store a special marker value (kTombstoneValue) to indicate deletion.
   * This allows us to track deletes in the same data structure as puts.
   * 
   * Why not just erase from the map?
   * - Need to remember that the key was deleted (to shadow older values in SSTables)
   * - Must write tombstones to disk during flush to maintain delete semantics
   * - Compaction can later remove both tombstone and older values
   */
  std::lock_guard<std::mutex> lock(mutex_);                      // Lock to protect shared state.
  
  auto it = map_.find(key);                                      // Check if key exists.
  if (it != map_.end()) {                                        // If updating existing key...
    approx_size_bytes_ -= it->second.size();                     // ...subtract old value size.
    approx_size_bytes_ += kTombstoneValue.size();                // ...add tombstone size.
  } else {                                                       // If inserting new tombstone...
    approx_size_bytes_ += key.size() + kTombstoneValue.size();   // ...add both key and tombstone size.
  }                                                              // End size tracking.
  
  map_[std::move(key)] = kTombstoneValue;                        // Store tombstone marker.
}                                                                // End Delete.

std::optional<std::string> MemTable::Get(const std::string& key) const { // Lookup a key.
  std::lock_guard<std::mutex> lock(mutex_);                               // Lock for safe read.
  const auto it = map_.find(key);                                         // Search the map.
  if (it == map_.end()) {                                                 // If missing...
    return std::nullopt;                                                  // ...return empty.
  }                                                                       // End missing case.
  
  // Check if it's a tombstone (deleted key).
  if (it->second == kTombstoneValue) {                                    // If tombstone marker...
    return std::nullopt;                                                  // ...treat as deleted (not found).
  }                                                                       // End tombstone check.
  
  return it->second;                                                      // Return a copy of the stored value.
}                                                                          // End Get.

std::size_t MemTable::Size() const {                             // Return number of entries.
  std::lock_guard<std::mutex> lock(mutex_);                      // Lock for safe read.
  return map_.size();                                            // Return entry count.
}                                                                // End Size.

std::size_t MemTable::ApproximateSizeBytes() const {             // Return approximate memory usage.
  std::lock_guard<std::mutex> lock(mutex_);                      // Lock for safe read.
  return approx_size_bytes_;                                     // Return tracked byte count.
}                                                                // End ApproximateSizeBytes.

std::vector<std::pair<std::string, std::string>> MemTable::GetAllSorted() const { // Export all entries.
  std::lock_guard<std::mutex> lock(mutex_);                                       // Lock for safe read.
  return std::vector<std::pair<std::string, std::string>>(map_.begin(), map_.end()); // Copy to vector.
}                                                                                  // End GetAllSorted.

void MemTable::Clear() {                                         // Clear all entries.
  std::lock_guard<std::mutex> lock(mutex_);                      // Lock for safe write.
  map_.clear();                                                  // Clear map.
  approx_size_bytes_ = 0;                                        // Reset size counter.
}                                                                // End Clear.

}  // namespace core_engine
