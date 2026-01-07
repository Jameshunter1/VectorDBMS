#include <core_engine/lsm/wal.hpp>

// Every line is commented (as requested) for the new LSM module.

#include <fstream>   // std::ofstream/std::ifstream for WAL I/O.
#include <ios>       // std::ios flags.
#include <limits>    // std::numeric_limits.

namespace core_engine {

Wal::Wal(std::filesystem::path path) : path_(std::move(path)) {} // Store WAL path.

Status Wal::OpenOrCreate() {                                   // Ensure the WAL file exists.
  std::ofstream out(path_, std::ios::binary | std::ios::app);  // Open for append (creates if missing).
  if (!out) {                                                  // If the open/create failed...
    return Status::IoError("Failed to open/create WAL");       // ...return an I/O error.
  }                                                           // End failure check.
  return Status::Ok();                                         // WAL file is ready.
}                                                              // End OpenOrCreate.

static void WriteU32LE(std::ofstream& out, std::uint32_t value) {   // Helper to write a 32-bit integer in little-endian.
  const unsigned char b0 = static_cast<unsigned char>((value >> 0) & 0xFFu);  // Lowest byte.
  const unsigned char b1 = static_cast<unsigned char>((value >> 8) & 0xFFu);  // Next byte.
  const unsigned char b2 = static_cast<unsigned char>((value >> 16) & 0xFFu); // Next byte.
  const unsigned char b3 = static_cast<unsigned char>((value >> 24) & 0xFFu); // Highest byte.
  out.put(static_cast<char>(b0));                                            // Write byte 0.
  out.put(static_cast<char>(b1));                                            // Write byte 1.
  out.put(static_cast<char>(b2));                                            // Write byte 2.
  out.put(static_cast<char>(b3));                                            // Write byte 3.
}                                                                           // End WriteU32LE.

static bool ReadU32LE(std::ifstream& in, std::uint32_t* out_value) {          // Helper to read a 32-bit little-endian integer.
  unsigned char bytes[4]{};                                                  // Buffer for 4 bytes.
  in.read(reinterpret_cast<char*>(bytes), 4);                                // Read exactly 4 bytes.
  if (!in) {                                                                 // If read failed...
    return false;                                                            // ...signal failure.
  }                                                                          // End read check.
  *out_value = (static_cast<std::uint32_t>(bytes[0]) << 0) |                 // Reconstruct value.
               (static_cast<std::uint32_t>(bytes[1]) << 8) |                 // ...
               (static_cast<std::uint32_t>(bytes[2]) << 16) |                // ...
               (static_cast<std::uint32_t>(bytes[3]) << 24);                 // ...
  return true;                                                               // Success.
}                                                                            // End ReadU32LE.

Status Wal::AppendPut(const std::string& key, const std::string& value) { // Append a Put record.
  std::ofstream out(path_, std::ios::binary | std::ios::app);             // Re-open in append mode (simple template).
  if (!out) {                                                             // If we cannot open the WAL...
    return Status::IoError("Failed to open WAL for append");             // ...return an I/O error.
  }                                                                        // End open check.

  const auto type = static_cast<std::uint8_t>(WalRecordType::kPut);        // Encode record type.
  const auto key_len = static_cast<std::uint32_t>(key.size());             // Encode key length (bytes).
  const auto value_len = static_cast<std::uint32_t>(value.size());         // Encode value length (bytes).

  out.write(reinterpret_cast<const char*>(&type), sizeof(type));           // Write 1-byte record type.
  WriteU32LE(out, key_len);                                               // Write key length (LE).
  WriteU32LE(out, value_len);                                             // Write value length (LE).
  out.write(key.data(), static_cast<std::streamsize>(key.size()));         // Write key bytes.
  out.write(value.data(), static_cast<std::streamsize>(value.size()));     // Write value bytes.

  if (!out) {                                                              // If any write failed...
    return Status::IoError("Failed to append WAL record");                // ...return an I/O error.
  }                                                                        // End write check.

  return Status::Ok();                                                     // Append succeeded.
}                                                                          // End AppendPut.

Status Wal::AppendDelete(const std::string& key) {                        // Append a Delete record (tombstone).
  /**
   * TOMBSTONE CONCEPT
   * 
   * Instead of immediately removing data from disk, we write a "tombstone"
   * marker that says "this key is deleted". This is much faster than
   * rewriting files, and it maintains the append-only property of the WAL.
   * 
   * During reads, if we find a tombstone for a key, we treat it as deleted.
   * During compaction, we can actually remove both the tombstone and old values.
   * 
   * File format for delete:
   * [1 byte: kDelete][4 bytes: key_len][4 bytes: 0 (no value)][key bytes]
   */
  std::ofstream out(path_, std::ios::binary | std::ios::app);             // Re-open in append mode.
  if (!out) {                                                             // If we cannot open the WAL...
    return Status::IoError("Failed to open WAL for append");             // ...return an I/O error.
  }                                                                        // End open check.

  const auto type = static_cast<std::uint8_t>(WalRecordType::kDelete);     // Encode record type as Delete.
  const auto key_len = static_cast<std::uint32_t>(key.size());             // Encode key length (bytes).
  const std::uint32_t value_len = 0;                                       // Deletes have no value (tombstone only).

  out.write(reinterpret_cast<const char*>(&type), sizeof(type));           // Write 1-byte record type.
  WriteU32LE(out, key_len);                                               // Write key length (LE).
  WriteU32LE(out, value_len);                                             // Write 0 for value length.
  out.write(key.data(), static_cast<std::streamsize>(key.size()));         // Write key bytes.
  // No value bytes to write for a delete

  if (!out) {                                                              // If any write failed...
    return Status::IoError("Failed to append WAL delete record");         // ...return an I/O error.
  }                                                                        // End write check.

  return Status::Ok();                                                     // Append succeeded.
}                                                                          // End AppendDelete.

Status Wal::Replay(const ReplayCallback& apply) {                            // Replay all WAL records.
  std::ifstream in(path_, std::ios::binary);                                 // Open for reading.
  if (!in) {                                                                 // If open failed...
    return Status::IoError("Failed to open WAL for replay");                // ...report I/O error.
  }                                                                          // End open check.

  while (true) {                                                            // Read records until EOF.
    std::uint8_t type_u8 = 0;                                               // Record type byte.
    in.read(reinterpret_cast<char*>(&type_u8), 1);                          // Read type.
    if (!in) {                                                              // If read failed...
      if (in.eof()) {                                                       // ...and we hit EOF cleanly...
        return Status::Ok();                                                // ...replay is complete.
      }                                                                     // End EOF check.
      return Status::Corruption("WAL truncated at record type");           // Otherwise, it's corruption.
    }                                                                       // End type read check.

    std::uint32_t key_len = 0;                                              // Key length.
    std::uint32_t value_len = 0;                                            // Value length.
    if (!ReadU32LE(in, &key_len)) {                                         // Read key length.
      return Status::Corruption("WAL truncated at key length");            // Report corruption.
    }                                                                       // End key_len read.
    if (!ReadU32LE(in, &value_len)) {                                       // Read value length.
      return Status::Corruption("WAL truncated at value length");          // Report corruption.
    }                                                                       // End value_len read.

    // Basic sanity limits to avoid unbounded allocations on corrupted files.
    constexpr std::uint32_t kMaxFieldSize = 64u * 1024u * 1024u;            // 64 MiB cap per field.
    if (key_len > kMaxFieldSize || value_len > kMaxFieldSize) {            // If lengths are insane...
      return Status::Corruption("WAL record length too large");            // ...treat as corruption.
    }                                                                       // End sanity check.

    std::string key;                                                        // Key buffer.
    std::string value;                                                      // Value buffer.
    key.resize(static_cast<std::size_t>(key_len));                           // Allocate key storage.
    value.resize(static_cast<std::size_t>(value_len));                       // Allocate value storage.

    in.read(key.data(), static_cast<std::streamsize>(key.size()));           // Read key bytes.
    if (!in) {                                                              // If read failed...
      return Status::Corruption("WAL truncated at key bytes");             // ...corruption.
    }                                                                       // End key read.

    in.read(value.data(), static_cast<std::streamsize>(value.size()));       // Read value bytes.
    if (!in) {                                                              // If read failed...
      return Status::Corruption("WAL truncated at value bytes");           // ...corruption.
    }                                                                       // End value read.

    const auto type = static_cast<WalRecordType>(type_u8);                   // Decode type enum.
    auto status = apply(type, std::move(key), std::move(value));             // Apply record to caller.
    if (!status.ok()) {                                                     // If apply failed...
      return status;                                                        // ...stop replay.
    }                                                                       // End apply check.
  }                                                                         // End while.
}                                                                           // End Replay.

}  // namespace core_engine
