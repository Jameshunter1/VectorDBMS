#pragma once

// SSTable: Sorted String Table — immutable on-disk sorted key-value file.
// This module handles writing (flush) and reading (scan/lookup) SSTables.
// Now includes Bloom filters for fast negative lookups.

#include <core_engine/common/status.hpp> // Status result type.
#include <core_engine/lsm/bloom_filter.hpp> // Bloom filter for fast key checks.

#include <cstdint>      // std::uint32_t.
#include <filesystem>   // std::filesystem::path.
#include <memory>       // std::unique_ptr.
#include <optional>     // std::optional.
#include <string>       // std::string.
#include <vector>       // std::vector.

namespace core_engine {

// SSTableWriter: builds a sorted SSTable file from key-value pairs.
// Usage:
//   1. Construct with target file path.
//   2. Call Add() for each key-value pair **in sorted order**.
//   3. Call Finish() to write the file and close.
class SSTableWriter {
public:
  explicit SSTableWriter(std::filesystem::path file_path); // Prepare to write to file_path.

  Status Open();                                           // Open file for writing.
  Status Add(const std::string& key, const std::string& value); // Append a sorted entry.
  Status Finish();                                         // Finalize and close the file.

private:
  std::filesystem::path file_path_;                        // Target SSTable file.
  std::vector<std::pair<std::string, std::string>> entries_; // Buffer entries before writing.
  bool is_open_ = false;                                   // Open state.
};                                                         // End SSTableWriter.

// SSTableReader: reads a sorted SSTable file.
// Supports sequential scan and binary-search lookup.
// Uses Bloom filter to quickly reject keys that don't exist.
class SSTableReader {
public:
  explicit SSTableReader(std::filesystem::path file_path); // Prepare to read from file_path.

  Status Open();                                           // Parse and validate SSTable file.
  std::optional<std::string> Get(const std::string& key);  // Lookup key via binary search.
  std::vector<std::pair<std::string, std::string>> GetAllSorted() const; // Return all entries (for compaction).

  const std::filesystem::path& file_path() const { return file_path_; } // Accessor.
  
  // Bloom filter statistics
  size_t GetBloomFilterChecks() const { return bloom_checks_; }
  size_t GetBloomFilterHits() const { return bloom_hits_; }
  size_t GetBloomFilterFalsePositives() const { return bloom_false_positives_; }

private:
  std::filesystem::path file_path_;                        // SSTable file path.
  std::vector<std::pair<std::string, std::string>> entries_; // In-memory sorted entries.
  std::unique_ptr<BloomFilter> bloom_filter_;              // Bloom filter for fast negative lookups.
  bool is_open_ = false;                                   // Open state.
  
  // Statistics (mutable so Get() can update them even though it's logically const)
  mutable size_t bloom_checks_ = 0;                        // Total Bloom filter checks.
  mutable size_t bloom_hits_ = 0;                          // Keys Bloom filter said "not present".
  mutable size_t bloom_false_positives_ = 0;               // Keys Bloom said "maybe" but weren't there.
};                                                         // End SSTableReader.

}  // namespace core_engine
