// core_engine/lib/lsm/bloom_filter.cpp
//
// Implementation of the Bloom filter probabilistic data structure.
//
// Key Concepts for Learners:
// 1. Bit Arrays: We store bits efficiently by packing 8 bits into each byte.
// 2. Hash Functions: Convert strings into numbers (indices into bit array).
// 3. Modulo Operator (%): Wraps large hash values into valid array indices.
// 4. Bitwise Operations: Manipulate individual bits using &, |, << operators.

#include <core_engine/lsm/bloom_filter.hpp>

#include <cstring>   // For memcpy
#include <functional> // For std::hash

namespace core_engine {

// Constructor: Initialize the bit array
//
// We allocate enough bytes to hold bits_count bits.
// Example: 10,000 bits needs 10,000/8 = 1,250 bytes.
//
BloomFilter::BloomFilter(size_t bits_count, size_t hash_count)
    : hash_count_(hash_count) {
  // Calculate how many bytes we need
  // Add 7 to round up (e.g., 9 bits needs 2 bytes, not 1)
  size_t num_bytes = (bits_count + 7) / 8;
  
  // Initialize all bytes to 0 (all bits start as 0)
  bits_.resize(num_bytes, 0);
}

// Add a key to the Bloom filter
//
// Process:
// 1. Hash the key K times to get K indices
// 2. Set all K bits to 1
//
void BloomFilter::Add(const std::string& key) {
  // Get K hash indices for this key
  auto indices = GetHashIndices(key);
  
  // Set each corresponding bit to 1
  for (size_t index : indices) {
    SetBit(index);
  }
}

// Check if a key might be present
//
// Process:
// 1. Hash the key K times to get K indices
// 2. Check if ALL K bits are 1
// 3. If any bit is 0, the key is definitely not present
// 4. If all bits are 1, the key *might* be present (could be false positive)
//
bool BloomFilter::MayContain(const std::string& key) const {
  // Get K hash indices for this key
  auto indices = GetHashIndices(key);
  
  // Check if ALL corresponding bits are set to 1
  for (size_t index : indices) {
    if (!GetBit(index)) {
      // Found a 0 bit → key definitely NOT present
      return false;
    }
  }
  
  // All bits were 1 → key MIGHT be present
  return true;
}

// Generate K hash indices using double hashing
//
// Double Hashing Technique:
// - Instead of computing K independent hash functions (slow),
// - We compute 2 hashes and combine them: hash_i = (h1 + i*h2) % M
// - This is much faster and works well in practice.
//
std::vector<size_t> BloomFilter::GetHashIndices(const std::string& key) const {
  // std::hash is a built-in C++ hash function
  // It converts a string into a large number (size_t)
  std::hash<std::string> hasher;
  
  // Compute two base hash values
  // We add a "salt" (constant) to get different hash values
  uint64_t hash1 = hasher(key);
  uint64_t hash2 = hasher(key + "salt");  // Different from hash1
  
  // Calculate total number of bits
  size_t total_bits = bits_.size() * 8;
  
  // Generate K hash indices using double hashing formula
  std::vector<size_t> indices;
  indices.reserve(hash_count_);  // Pre-allocate for efficiency
  
  for (size_t i = 0; i < hash_count_; ++i) {
    // Combine the two hashes: hash_i = (hash1 + i * hash2) % M
    // The % operator ensures the result is within [0, total_bits)
    uint64_t combined_hash = hash1 + i * hash2;
    size_t index = combined_hash % total_bits;
    indices.push_back(index);
  }
  
  return indices;
}

// Set a bit to 1
//
// Bit Manipulation Explanation:
// - bit_index = 13 (we want to set the 13th bit)
// - byte_index = 13 / 8 = 1 (bit 13 is in the 2nd byte)
// - bit_offset = 13 % 8 = 5 (it's the 6th bit in that byte)
// - mask = 1 << 5 = 0b00100000 (a byte with only bit 5 set)
// - bits_[1] |= mask (OR operation sets that bit to 1)
//
void BloomFilter::SetBit(size_t bit_index) {
  // Which byte contains this bit?
  size_t byte_index = bit_index / 8;
  
  // Which bit within that byte? (0-7)
  size_t bit_offset = bit_index % 8;
  
  // Create a mask with only that bit set to 1
  // Example: bit_offset=5 → mask = 0b00100000
  uint8_t mask = static_cast<uint8_t>(1 << bit_offset);
  
  // Set the bit using OR operation
  // OR keeps existing 1s and sets new 1s
  bits_[byte_index] |= mask;
}

// Get a bit value (0 or 1)
//
// Bit Manipulation Explanation:
// - Similar to SetBit, but we READ instead of WRITE
// - We use AND (&) to test if a bit is set
// - If (byte & mask) is non-zero, the bit is 1
//
bool BloomFilter::GetBit(size_t bit_index) const {
  size_t byte_index = bit_index / 8;
  size_t bit_offset = bit_index % 8;
  
  // Create a mask for testing
  uint8_t mask = static_cast<uint8_t>(1 << bit_offset);
  
  // Test the bit using AND operation
  // If the result is non-zero, the bit is 1
  return (bits_[byte_index] & mask) != 0;
}

// Serialize to bytes for storage
//
// Format:
// [8 bytes: bits_count] [8 bytes: hash_count] [N bytes: bit array]
//
// This allows us to save the Bloom filter to disk and load it later.
//
std::vector<uint8_t> BloomFilter::Serialize() const {
  // Calculate total size
  size_t total_size = sizeof(uint64_t) * 2 + bits_.size();
  
  // Create output buffer
  std::vector<uint8_t> data;
  data.reserve(total_size);
  
  // Write bits_count (8 bytes)
  uint64_t bits_count = bits_.size() * 8;
  const uint8_t* bits_count_ptr = reinterpret_cast<const uint8_t*>(&bits_count);
  data.insert(data.end(), bits_count_ptr, bits_count_ptr + sizeof(uint64_t));
  
  // Write hash_count (8 bytes)
  uint64_t hash_count = hash_count_;
  const uint8_t* hash_count_ptr = reinterpret_cast<const uint8_t*>(&hash_count);
  data.insert(data.end(), hash_count_ptr, hash_count_ptr + sizeof(uint64_t));
  
  // Write bit array
  data.insert(data.end(), bits_.begin(), bits_.end());
  
  return data;
}

// Deserialize from bytes
//
// This is the reverse of Serialize().
// We read the format: [bits_count][hash_count][bit_array]
//
BloomFilter* BloomFilter::Deserialize(const std::vector<uint8_t>& data) {
  // Check minimum size (16 bytes for header)
  if (data.size() < sizeof(uint64_t) * 2) {
    return nullptr;  // Invalid data
  }
  
  // Read bits_count (first 8 bytes)
  uint64_t bits_count;
  std::memcpy(&bits_count, data.data(), sizeof(uint64_t));
  
  // Read hash_count (next 8 bytes)
  uint64_t hash_count;
  std::memcpy(&hash_count, data.data() + sizeof(uint64_t), sizeof(uint64_t));
  
  // Calculate expected bit array size
  size_t expected_bytes = (bits_count + 7) / 8;
  size_t header_size = sizeof(uint64_t) * 2;
  
  // Validate data size
  if (data.size() != header_size + expected_bytes) {
    return nullptr;  // Size mismatch
  }
  
  // Create new Bloom filter
  // Note: We use 'new' here because we return a pointer
  // The caller is responsible for deleting this object
  BloomFilter* filter = new BloomFilter(bits_count, hash_count);
  
  // Copy bit array data
  std::memcpy(filter->bits_.data(), 
              data.data() + header_size, 
              expected_bytes);
  
  return filter;
}

}  // namespace core_engine
