// core_engine/include/core_engine/lsm/bloom_filter.hpp
//
// Purpose:
// - A space-efficient probabilistic data structure for set membership testing.
// - Answers "is X definitely NOT in the set?" with 100% accuracy.
// - May have false positives (says "maybe yes" when answer is "no").
// - Never has false negatives (if it says "no", it's definitely not there).
//
// For C++ learners:
// - This demonstrates bit manipulation and hashing.
// - Shows how probabilistic algorithms trade accuracy for space efficiency.
// - A 1MB Bloom filter can represent millions of keys with <1% false positive rate.
//
// How it works:
// 1. Initialize a bit array of size M (all bits set to 0).
// 2. Use K different hash functions.
// 3. To ADD a key: hash it K times, set those K bits to 1.
// 4. To CHECK a key: hash it K times, if ALL K bits are 1, return "maybe present".
//                     If ANY bit is 0, return "definitely not present".
//
// Why false positives happen:
// - Multiple keys can hash to overlapping bit positions.
// - Eventually, unrelated keys create a pattern that matches.
//
// Why no false negatives:
// - If a key was added, its K bits were definitely set to 1.
// - We never clear bits, so they remain 1 forever.

#pragma once

#include <cstddef>   // For size_t
#include <cstdint>   // For uint8_t, uint64_t
#include <string>    // For std::string
#include <vector>    // For std::vector

namespace core_engine {

// BloomFilter: Space-efficient set membership checker
//
// Usage:
//   BloomFilter filter(10000, 3);  // 10K bits, 3 hash functions
//   filter.Add("alice");
//   filter.Add("bob");
//   
//   filter.MayContain("alice")  → true  (definitely added)
//   filter.MayContain("bob")    → true  (definitely added)
//   filter.MayContain("charlie") → false (definitely not added)
//   filter.MayContain("xyz")    → true  (false positive! wasn't added)
//
class BloomFilter {
public:
  // Constructor
  //
  // Parameters:
  //   bits_count: Total number of bits in the filter (M).
  //               Larger = lower false positive rate but more memory.
  //               Rule of thumb: 10 bits per key for ~1% false positive rate.
  //
  //   hash_count: Number of hash functions to use (K).
  //               More hashes = better accuracy but slower operations.
  //               Optimal K ≈ 0.7 * (M / N) where N = expected key count.
  //               Typical values: 3-7 hash functions.
  //
  BloomFilter(size_t bits_count, size_t hash_count);

  // Add a key to the Bloom filter
  //
  // This sets K bits to 1 (one for each hash function).
  // Idempotent: adding the same key twice has no additional effect.
  //
  void Add(const std::string& key);

  // Check if a key might be in the set
  //
  // Returns:
  //   true:  Key MIGHT be present (or false positive).
  //   false: Key is DEFINITELY NOT present.
  //
  bool MayContain(const std::string& key) const;

  // Serialize the Bloom filter to bytes (for storing in SSTable)
  //
  // Format: [bits_count(8 bytes)][hash_count(8 bytes)][bit_array...]
  //
  std::vector<uint8_t> Serialize() const;

  // Deserialize a Bloom filter from bytes (for loading from SSTable)
  //
  // Returns: A new BloomFilter if successful, or nullptr on error.
  //
  static BloomFilter* Deserialize(const std::vector<uint8_t>& data);

  // Get statistics
  size_t GetBitsCount() const { return bits_.size() * 8; }
  size_t GetHashCount() const { return hash_count_; }
  size_t GetSizeBytes() const { return bits_.size(); }

private:
  // Generate the K hash values for a key
  //
  // We use a variant of "double hashing":
  //   hash_i = (hash1 + i * hash2) % M
  //
  // This is faster than running K independent hash functions.
  //
  std::vector<size_t> GetHashIndices(const std::string& key) const;

  // Set a bit at the given index
  //
  // Bit manipulation:
  //   byte_index = bit_index / 8       (which byte contains this bit?)
  //   bit_offset = bit_index % 8       (which bit within that byte?)
  //   bits_[byte_index] |= (1 << bit_offset)  (set bit to 1 using OR)
  //
  void SetBit(size_t bit_index);

  // Get a bit at the given index
  //
  // Bit manipulation:
  //   byte_index = bit_index / 8
  //   bit_offset = bit_index % 8
  //   return (bits_[byte_index] & (1 << bit_offset)) != 0  (test bit using AND)
  //
  bool GetBit(size_t bit_index) const;

  // Member variables:
  std::vector<uint8_t> bits_;  // Bit array (packed into bytes)
  size_t hash_count_;           // Number of hash functions (K)
};

}  // namespace core_engine
