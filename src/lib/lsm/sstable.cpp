#include <core_engine/lsm/sstable.hpp>
#include <core_engine/lsm/memtable.hpp>  // For kTombstoneValue constant.

#include <algorithm>  // std::lower_bound, std::sort.
#include <fstream>    // std::ofstream, std::ifstream.
#include <ios>        // std::ios flags.

namespace core_engine {

// Helper: write 32-bit little-endian integer.
static void WriteU32LE(std::ofstream& out, std::uint32_t value) {
  const unsigned char b0 = static_cast<unsigned char>((value >> 0) & 0xFFu);
  const unsigned char b1 = static_cast<unsigned char>((value >> 8) & 0xFFu);
  const unsigned char b2 = static_cast<unsigned char>((value >> 16) & 0xFFu);
  const unsigned char b3 = static_cast<unsigned char>((value >> 24) & 0xFFu);
  out.put(static_cast<char>(b0));
  out.put(static_cast<char>(b1));
  out.put(static_cast<char>(b2));
  out.put(static_cast<char>(b3));
}

// Helper: read 32-bit little-endian integer.
static bool ReadU32LE(std::ifstream& in, std::uint32_t* out_value) {
  unsigned char bytes[4]{};
  in.read(reinterpret_cast<char*>(bytes), 4);
  if (!in) {
    return false;
  }
  *out_value = (static_cast<std::uint32_t>(bytes[0]) << 0) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
  return true;
}

// SSTableWriter implementation.

SSTableWriter::SSTableWriter(std::filesystem::path file_path)
    : file_path_(std::move(file_path)) {}

Status SSTableWriter::Open() {
  if (is_open_) {
    return Status::Internal("SSTableWriter already open");
  }
  is_open_ = true;
  return Status::Ok();
}

Status SSTableWriter::Add(const std::string& key, const std::string& value) {
  if (!is_open_) {
    return Status::Internal("SSTableWriter not open");
  }
  entries_.emplace_back(key, value);
  return Status::Ok();
}

Status SSTableWriter::Finish() {
  if (!is_open_) {
    return Status::Internal("SSTableWriter not open");
  }

  // Sort entries by key (LSM invariant: SSTables are sorted).
  std::sort(entries_.begin(), entries_.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  // Build Bloom filter for fast negative lookups
  // Parameters:
  //   bits_per_key = 10 → ~1% false positive rate
  //   hash_count = 3 → good balance between speed and accuracy
  size_t bits_per_key = 10;
  size_t hash_count = 3;
  size_t total_bits = entries_.size() * bits_per_key;
  BloomFilter bloom(total_bits, hash_count);
  
  // Add all keys to the Bloom filter
  for (const auto& [key, value] : entries_) {
    bloom.Add(key);
  }
  
  // Serialize the Bloom filter
  auto bloom_data = bloom.Serialize();

  // Write file.
  std::ofstream out(file_path_, std::ios::binary | std::ios::trunc);
  if (!out) {
    return Status::IoError("Failed to open SSTable for writing");
  }

  // Header: magic "SSTB" + entry count + bloom filter size.
  out.write("SSTB", 4);
  WriteU32LE(out, static_cast<std::uint32_t>(entries_.size()));
  WriteU32LE(out, static_cast<std::uint32_t>(bloom_data.size()));
  
  // Write Bloom filter data
  out.write(reinterpret_cast<const char*>(bloom_data.data()), 
            static_cast<std::streamsize>(bloom_data.size()));

  // Write entries.
  for (const auto& [key, value] : entries_) {
    WriteU32LE(out, static_cast<std::uint32_t>(key.size()));
    WriteU32LE(out, static_cast<std::uint32_t>(value.size()));
    out.write(key.data(), static_cast<std::streamsize>(key.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
  }

  if (!out) {
    return Status::IoError("Failed to write SSTable");
  }

  is_open_ = false;
  return Status::Ok();
}

// SSTableReader implementation.

SSTableReader::SSTableReader(std::filesystem::path file_path)
    : file_path_(std::move(file_path)) {}

Status SSTableReader::Open() {
  if (is_open_) {
    return Status::Internal("SSTableReader already open");
  }

  std::ifstream in(file_path_, std::ios::binary);
  if (!in) {
    return Status::IoError("Failed to open SSTable for reading");
  }

  // Read header: magic + entry count.
  char magic[4]{};
  in.read(magic, 4);
  if (!in || std::string(magic, 4) != "SSTB") {
    return Status::Corruption("Invalid SSTable magic");
  }

  std::uint32_t entry_count = 0;
  if (!ReadU32LE(in, &entry_count)) {
    return Status::Corruption("Failed to read SSTable entry count");
  }
  
  // Try to read Bloom filter size (new format)
  // If this fails, it's an old-format SSTable without Bloom filter
  std::uint32_t bloom_size = 0;
  auto saved_pos = in.tellg();  // Save position in case we need to rewind
  
  if (ReadU32LE(in, &bloom_size) && bloom_size > 0 && bloom_size < 100*1024*1024) {  // Sanity check: < 100MB
    // New format with Bloom filter
    std::vector<uint8_t> bloom_data(bloom_size);
    in.read(reinterpret_cast<char*>(bloom_data.data()), 
            static_cast<std::streamsize>(bloom_size));
    if (in) {
      // Successfully read Bloom filter
      bloom_filter_.reset(BloomFilter::Deserialize(bloom_data));
      // If deserialization fails, that's OK - we'll just not use Bloom filter
    } else {
      // Failed to read Bloom data - rewind and treat as old format
      in.clear();
      in.seekg(saved_pos);
    }
  } else {
    // Old format without Bloom filter - rewind
    in.clear();
    in.seekg(saved_pos);
  }

  // Read entries.
  entries_.reserve(entry_count);
  for (std::uint32_t i = 0; i < entry_count; ++i) {
    std::uint32_t key_len = 0;
    std::uint32_t value_len = 0;
    if (!ReadU32LE(in, &key_len) || !ReadU32LE(in, &value_len)) {
      return Status::Corruption("Failed to read SSTable entry lengths");
    }

    std::string key;
    std::string value;
    key.resize(key_len);
    value.resize(value_len);

    in.read(key.data(), static_cast<std::streamsize>(key.size()));
    if (!in) {
      return Status::Corruption("Failed to read SSTable key");
    }

    in.read(value.data(), static_cast<std::streamsize>(value.size()));
    if (!in) {
      return Status::Corruption("Failed to read SSTable value");
    }

    entries_.emplace_back(std::move(key), std::move(value));
  }

  is_open_ = true;
  return Status::Ok();
}

std::optional<std::string> SSTableReader::Get(const std::string& key) {
  if (!is_open_) {
    return std::nullopt;
  }
  
  /**
   * BLOOM FILTER OPTIMIZATION
   * 
   * Check Bloom filter first to quickly reject keys that don't exist.
   * This avoids expensive binary searches for non-existent keys.
   */
  bloom_checks_++;
  if (bloom_filter_ && !bloom_filter_->MayContain(key)) {
    bloom_hits_++;  // Bloom filter saved us a binary search!
    return std::nullopt;
  }

  // Binary search for key.
  auto it = std::lower_bound(
      entries_.begin(), entries_.end(), key,
      [](const auto& entry, const std::string& k) { return entry.first < k; });

  if (it != entries_.end() && it->first == key) {
    /**
     * TOMBSTONE CHECK
     * 
     * If the value is a tombstone marker, treat it as "key not found".
     * Tombstones shadow older values in older SSTables, implementing delete semantics.
     */
    if (it->second == MemTable::kTombstoneValue) {
      return std::nullopt;  // Key was deleted.
    }
    return it->second;  // Return the actual value.
  }
  
  // Key not found - if Bloom filter said "maybe", this is a false positive
  if (bloom_filter_) {
    bloom_false_positives_++;
  }

  return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> SSTableReader::GetAllSorted() const {
  return entries_;  // Already sorted.
}

}  // namespace core_engine
