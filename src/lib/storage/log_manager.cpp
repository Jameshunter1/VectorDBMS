// core_engine/storage/log_manager.cpp
//
// Implementation of Write-Ahead Logging (WAL) system
// Year 1 Q4: Durability and recovery

#include <core_engine/common/logger.hpp>
#include <core_engine/storage/log_manager.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <io.h> // for _commit, _fileno
#include <stdio.h>
#else
#include <unistd.h> // for fsync, fileno
#endif

namespace core_engine {

// ============================================================================
// LogRecord Serialization
// ============================================================================

void LogRecord::SerializeBase(std::vector<std::byte>& buffer) const {
  // Format: [LSN (8)][TxnId (8)][PrevLSN (8)][Type (1)]
  buffer.resize(sizeof(LSN) + sizeof(TxnId) + sizeof(LSN) + sizeof(LogRecordType));

  std::size_t offset = 0;
  std::memcpy(buffer.data() + offset, &lsn, sizeof(LSN));
  offset += sizeof(LSN);

  std::memcpy(buffer.data() + offset, &txn_id, sizeof(TxnId));
  offset += sizeof(TxnId);

  std::memcpy(buffer.data() + offset, &prev_lsn, sizeof(LSN));
  offset += sizeof(LSN);

  std::memcpy(buffer.data() + offset, &type, sizeof(LogRecordType));
}

std::vector<std::byte> BeginLogRecord::Serialize() const {
  std::vector<std::byte> buffer;
  SerializeBase(buffer);
  return buffer;
}

std::vector<std::byte> CommitLogRecord::Serialize() const {
  std::vector<std::byte> buffer;
  SerializeBase(buffer);
  return buffer;
}

std::vector<std::byte> AbortLogRecord::Serialize() const {
  std::vector<std::byte> buffer;
  SerializeBase(buffer);
  return buffer;
}

std::vector<std::byte> UpdateLogRecord::Serialize() const {
  std::vector<std::byte> buffer;
  SerializeBase(buffer);

  // Add update-specific fields: [PageId][Offset][Length][OldData][NewData]
  std::size_t base_size = buffer.size();
  std::size_t total_size =
      base_size + sizeof(PageId) + sizeof(std::size_t) * 2 + old_data.size() + new_data.size();
  buffer.resize(total_size);

  std::size_t write_offset = base_size;
  std::memcpy(buffer.data() + write_offset, &page_id, sizeof(PageId));
  write_offset += sizeof(PageId);

  std::memcpy(buffer.data() + write_offset, &this->offset, sizeof(std::size_t));
  write_offset += sizeof(std::size_t);

  std::memcpy(buffer.data() + write_offset, &length, sizeof(std::size_t));
  write_offset += sizeof(std::size_t);

  if (!old_data.empty()) {
    std::memcpy(buffer.data() + write_offset, old_data.data(), old_data.size());
    write_offset += old_data.size();
  }

  if (!new_data.empty()) {
    std::memcpy(buffer.data() + write_offset, new_data.data(), new_data.size());
  }

  return buffer;
}

std::vector<std::byte> CLRLogRecord::Serialize() const {
  std::vector<std::byte> buffer;
  SerializeBase(buffer);

  // Add CLR-specific fields: [PageId][Offset][Length][UndoData][UndoNextLSN]
  std::size_t base_size = buffer.size();
  std::size_t total_size =
      base_size + sizeof(PageId) + sizeof(std::size_t) * 2 + undo_data.size() + sizeof(LSN);
  buffer.resize(total_size);

  std::size_t write_offset = base_size;
  std::memcpy(buffer.data() + write_offset, &page_id, sizeof(PageId));
  write_offset += sizeof(PageId);

  std::memcpy(buffer.data() + write_offset, &this->offset, sizeof(std::size_t));
  write_offset += sizeof(std::size_t);

  std::memcpy(buffer.data() + write_offset, &length, sizeof(std::size_t));
  write_offset += sizeof(std::size_t);

  if (!undo_data.empty()) {
    std::memcpy(buffer.data() + write_offset, undo_data.data(), undo_data.size());
    write_offset += undo_data.size();
  }

  std::memcpy(buffer.data() + write_offset, &undo_next_lsn, sizeof(LSN));

  return buffer;
}

std::vector<std::byte> CheckpointLogRecord::Serialize() const {
  std::vector<std::byte> buffer;
  SerializeBase(buffer);

  // Add checkpoint-specific fields: [NumTxns][TxnId1][TxnId2]...
  std::size_t base_size = buffer.size();
  std::size_t num_txns = active_txns.size();
  std::size_t total_size = base_size + sizeof(std::size_t) + num_txns * sizeof(TxnId);
  buffer.resize(total_size);

  std::size_t offset = base_size;
  std::memcpy(buffer.data() + offset, &num_txns, sizeof(std::size_t));
  offset += sizeof(std::size_t);

  for (TxnId txn : active_txns) {
    std::memcpy(buffer.data() + offset, &txn, sizeof(TxnId));
    offset += sizeof(TxnId);
  }

  return buffer;
}

// ============================================================================
// LogManager Implementation
// ============================================================================

LogManager::LogManager(const std::string& log_file)
    : log_file_path_(log_file), next_lsn_(1), log_offset_(0) {

  // Open log file (create if doesn't exist)
  log_stream_.open(log_file, std::ios::binary | std::ios::in | std::ios::out);

  if (!log_stream_.is_open()) {
    // File doesn't exist - create it
    log_stream_.clear();
    log_stream_.open(log_file, std::ios::binary | std::ios::out);
    log_stream_.close();
    log_stream_.open(log_file, std::ios::binary | std::ios::in | std::ios::out);
  }

  if (!log_stream_.is_open()) {
    Log(LogLevel::kError, "Failed to open log file: " + log_file);
    throw std::runtime_error("Failed to open log file");
  }

  // Seek to end to find current offset
  log_stream_.seekg(0, std::ios::end);
  log_offset_ = log_stream_.tellg();

  // If log file has content, scan to determine next LSN
  if (log_offset_ > 0) {
    // TODO: Scan log to find maximum LSN and set next_lsn_ = max_lsn + 1
    // For now, assume empty log or recovery will handle this
  }

  Log(LogLevel::kDebug,
      "LogManager initialized, log file: " + log_file + ", offset: " + std::to_string(log_offset_));
}

LogManager::~LogManager() {
  // Flush and close log file
  if (log_stream_.is_open()) {
    ForceFlush();
    log_stream_.close();
  }
  Log(LogLevel::kDebug, "LogManager destroyed");
}

LSN LogManager::AppendBeginRecord(TxnId txn_id, LSN prev_lsn) {
  auto record = std::make_unique<BeginLogRecord>(txn_id, prev_lsn);
  return AppendLogRecord(std::move(record));
}

LSN LogManager::AppendCommitRecord(TxnId txn_id, LSN prev_lsn) {
  auto record = std::make_unique<CommitLogRecord>(txn_id, prev_lsn);
  return AppendLogRecord(std::move(record));
}

LSN LogManager::AppendAbortRecord(TxnId txn_id, LSN prev_lsn) {
  auto record = std::make_unique<AbortLogRecord>(txn_id, prev_lsn);
  return AppendLogRecord(std::move(record));
}

LSN LogManager::AppendUpdateRecord(TxnId txn_id, LSN prev_lsn, PageId page_id, std::size_t offset,
                                   std::size_t length, const std::byte* old_data,
                                   const std::byte* new_data) {
  auto record = std::make_unique<UpdateLogRecord>(txn_id, prev_lsn, page_id, offset, length,
                                                  old_data, new_data);
  return AppendLogRecord(std::move(record));
}

LSN LogManager::AppendCLRRecord(TxnId txn_id, LSN prev_lsn, PageId page_id, std::size_t offset,
                                std::size_t length, const std::byte* undo_data, LSN undo_next_lsn) {
  auto record = std::make_unique<CLRLogRecord>(txn_id, prev_lsn, page_id, offset, length, undo_data,
                                               undo_next_lsn);
  return AppendLogRecord(std::move(record));
}

LSN LogManager::AppendCheckpointRecord(const std::vector<TxnId>& active_txns) {
  auto record = std::make_unique<CheckpointLogRecord>();
  record->active_txns = active_txns;
  return AppendLogRecord(std::move(record));
}

LSN LogManager::AppendLogRecord(std::unique_ptr<LogRecord> record) {
  std::lock_guard<std::mutex> lock(latch_);

  // Assign LSN
  record->lsn = next_lsn_++;

  // Serialize record
  auto data = record->Serialize();

  // Write record size first (for reading back)
  std::uint32_t size = static_cast<std::uint32_t>(data.size());

  // Seek to end of log
  log_stream_.seekp(0, std::ios::end);

  // Write [size][data]
  log_stream_.write(reinterpret_cast<const char*>(&size), sizeof(size));
  log_stream_.write(reinterpret_cast<const char*>(data.data()), data.size());

  // Update offset
  log_offset_ += sizeof(size) + data.size();

  // Verbose debug logging disabled for performance (floods logs during benchmarks)
  // Log(LogLevel::kDebug, "Appended log record LSN=" + std::to_string(record->lsn) +
  //                           " Type=" + std::to_string(static_cast<int>(record->type)) +
  //                           " Size=" + std::to_string(size));

  return record->lsn;
}

Status LogManager::ForceFlush() {
  std::lock_guard<std::mutex> lock(latch_);

  if (!log_stream_.is_open()) {
    return Status::IoError("Log file not open");
  }

  // Flush C++ stream buffers
  log_stream_.flush();

  // Note: std::fstream doesn't provide direct access to file descriptor for fsync.
  // For full durability, the WAL file should be opened with O_SYNC or use FILE* instead.
  // For now, we rely on OS buffering (acceptable for v1.x, will improve in Year 2).
  // The flush() above ensures C++ buffers are written to OS, but OS may still buffer.

  // TODO (Year 2): Switch to DiskManager-style FILE* with explicit _commit()/_fsync()
  // for guaranteed durability. Current approach is acceptable for testing.

  Log(LogLevel::kDebug, "Log flushed to disk");
  return Status::Ok();
}

Status LogManager::ScanForward(LSN start_lsn, std::function<void(const LogRecord&)> callback) {
  std::lock_guard<std::mutex> lock(latch_);

  // Seek to beginning of log
  log_stream_.seekg(0, std::ios::beg);

  while (log_stream_.good() && !log_stream_.eof()) {
    // Read record size
    std::uint32_t size = 0;
    log_stream_.read(reinterpret_cast<char*>(&size), sizeof(size));

    if (log_stream_.gcount() != sizeof(size)) {
      break; // End of log
    }

    // Read record data
    std::vector<std::byte> data(size);
    log_stream_.read(reinterpret_cast<char*>(data.data()), size);

    if (log_stream_.gcount() != static_cast<std::streamsize>(size)) {
      return Status::Corruption("Incomplete log record");
    }

    // Deserialize and check LSN
    auto record = LogRecord::Deserialize(data.data(), size);
    if (record && record->lsn >= start_lsn) {
      callback(*record);
    }
  }

  return Status::Ok();
}

Status LogManager::ScanBackward(LSN start_lsn, std::function<void(const LogRecord&)> callback) {
  // Suppress unused parameter warnings for unimplemented function
  (void)start_lsn;
  (void)callback;
  // TODO: Implement backward scan (requires storing record offsets or scanning forward first)
  return Status::Unimplemented("Backward scan not yet implemented");
}

std::unique_ptr<LogRecord> LogManager::GetLogRecord(LSN lsn) {
  // TODO: Implement LSN -> offset mapping for efficient lookup
  // For now, scan forward to find the record
  std::unique_ptr<LogRecord> result = nullptr;

  ScanForward(lsn, [&](const LogRecord& record) {
    if (record.lsn == lsn && !result) {
      // Found the record - need to copy it
      // This is inefficient but works for now
      result = nullptr; // TODO: Clone the record
    }
  });

  return result;
}

// ============================================================================
// LogRecord Deserialization (Stub Implementation)
// ============================================================================

std::unique_ptr<LogRecord> LogRecord::Deserialize(const std::byte* data, std::size_t size) {
  // TODO: Implement proper deserialization
  // For now, just parse the base fields to determine type

  if (size < sizeof(LSN) + sizeof(TxnId) + sizeof(LSN) + sizeof(LogRecordType)) {
    return nullptr; // Invalid record
  }

  std::size_t offset = 0;
  LSN lsn;
  TxnId txn_id;
  LSN prev_lsn;
  LogRecordType type;

  std::memcpy(&lsn, data + offset, sizeof(LSN));
  offset += sizeof(LSN);

  std::memcpy(&txn_id, data + offset, sizeof(TxnId));
  offset += sizeof(TxnId);

  std::memcpy(&prev_lsn, data + offset, sizeof(LSN));
  offset += sizeof(LSN);

  std::memcpy(&type, data + offset, sizeof(LogRecordType));

  // Create appropriate record type
  std::unique_ptr<LogRecord> record;

  switch (type) {
  case LogRecordType::kBegin:
    record = std::make_unique<BeginLogRecord>(txn_id, prev_lsn);
    break;
  case LogRecordType::kCommit:
    record = std::make_unique<CommitLogRecord>(txn_id, prev_lsn);
    break;
  case LogRecordType::kAbort:
    record = std::make_unique<AbortLogRecord>(txn_id, prev_lsn);
    break;
  // TODO: Deserialize Update, CLR, Checkpoint records
  default:
    return nullptr;
  }

  if (record) {
    record->lsn = lsn;
  }

  return record;
}

} // namespace core_engine
