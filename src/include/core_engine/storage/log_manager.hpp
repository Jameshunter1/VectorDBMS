#pragma once

// core_engine/storage/log_manager.hpp
//
// Purpose:
// - Write-Ahead Logging (WAL) for ACID compliance and crash recovery
// - Year 1 Q4 milestone: Durability and recovery
//
// Design decisions:
// - ARIES-style logging (Algorithms for Recovery and Isolation Exploiting Semantics)
// - Append-only log file (sequential writes for performance)
// - Log sequence numbers (LSN) for ordering
// - Log records written before data pages (write-ahead rule)
// - Force log on commit (durability guarantee)
//
// Architecture:
// - LogManager: Appends log records, manages LSN, flushes to disk
// - LogRecord types: Begin, Update, Commit, Abort, CLR (Compensation Log Record)
// - Recovery: Analysis -> Redo -> Undo phases

#include <core_engine/common/status.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/page.hpp>

#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace core_engine {

// Log Sequence Number: Unique identifier for each log record
using LSN = std::uint64_t;
constexpr LSN kInvalidLSN = 0;

// Transaction ID
using TxnId = std::uint64_t;
constexpr TxnId kInvalidTxnId = 0;

// ============================================================================
// Log Record Types
// ============================================================================

enum class LogRecordType : std::uint8_t {
  kInvalid = 0,
  kBegin = 1,     // Transaction start
  kCommit = 2,    // Transaction commit
  kAbort = 3,     // Transaction abort
  kUpdate = 4,    // Data modification (before/after images)
  kCLR = 5,       // Compensation Log Record (undo action)
  kCheckpoint = 6 // Checkpoint marker
};

// Base log record structure
// All log records have: LSN, TxnId, PrevLSN, Type
struct LogRecord {
  LSN lsn = kInvalidLSN;        // Unique log sequence number
  TxnId txn_id = kInvalidTxnId; // Transaction that created this record
  LSN prev_lsn = kInvalidLSN;   // Previous LSN for this transaction
  LogRecordType type = LogRecordType::kInvalid;

  virtual ~LogRecord() = default;

  // Serialize record to bytes for writing to disk
  virtual std::vector<std::byte> Serialize() const = 0;

  // Deserialize record from bytes
  static std::unique_ptr<LogRecord> Deserialize(const std::byte* data, std::size_t size);

protected:
  // Helper to serialize base fields
  void SerializeBase(std::vector<std::byte>& buffer) const;
};

// Begin transaction record
struct BeginLogRecord : public LogRecord {
  BeginLogRecord(TxnId txn_id, LSN prev_lsn) {
    this->txn_id = txn_id;
    this->prev_lsn = prev_lsn;
    this->type = LogRecordType::kBegin;
  }

  std::vector<std::byte> Serialize() const override;
};

// Commit transaction record
struct CommitLogRecord : public LogRecord {
  CommitLogRecord(TxnId txn_id, LSN prev_lsn) {
    this->txn_id = txn_id;
    this->prev_lsn = prev_lsn;
    this->type = LogRecordType::kCommit;
  }

  std::vector<std::byte> Serialize() const override;
};

// Abort transaction record
struct AbortLogRecord : public LogRecord {
  AbortLogRecord(TxnId txn_id, LSN prev_lsn) {
    this->txn_id = txn_id;
    this->prev_lsn = prev_lsn;
    this->type = LogRecordType::kAbort;
  }

  std::vector<std::byte> Serialize() const override;
};

// Update record: Records a change to a page
struct UpdateLogRecord : public LogRecord {
  PageId page_id = kInvalidPageId;
  std::size_t offset = 0;          // Offset within page
  std::size_t length = 0;          // Number of bytes changed
  std::vector<std::byte> old_data; // Before image
  std::vector<std::byte> new_data; // After image

  UpdateLogRecord(TxnId txn_id, LSN prev_lsn, PageId page_id, std::size_t offset,
                  std::size_t length, const std::byte* old_val, const std::byte* new_val) {
    this->txn_id = txn_id;
    this->prev_lsn = prev_lsn;
    this->page_id = page_id;
    this->offset = offset;
    this->length = length;
    this->type = LogRecordType::kUpdate;

    if (old_val) {
      old_data.assign(old_val, old_val + length);
    }
    if (new_val) {
      new_data.assign(new_val, new_val + length);
    }
  }

  std::vector<std::byte> Serialize() const override;
};

// Compensation Log Record: Records undo of an update
struct CLRLogRecord : public LogRecord {
  PageId page_id = kInvalidPageId;
  std::size_t offset = 0;
  std::size_t length = 0;
  std::vector<std::byte> undo_data; // Data to restore
  LSN undo_next_lsn = kInvalidLSN;  // Next LSN to undo

  CLRLogRecord(TxnId txn_id, LSN prev_lsn, PageId page_id, std::size_t offset, std::size_t length,
               const std::byte* undo_val, LSN undo_next) {
    this->txn_id = txn_id;
    this->prev_lsn = prev_lsn;
    this->page_id = page_id;
    this->offset = offset;
    this->length = length;
    this->undo_next_lsn = undo_next;
    this->type = LogRecordType::kCLR;

    if (undo_val) {
      undo_data.assign(undo_val, undo_val + length);
    }
  }

  std::vector<std::byte> Serialize() const override;
};

// Checkpoint record: Marks point for recovery
struct CheckpointLogRecord : public LogRecord {
  std::vector<TxnId> active_txns; // Active transactions at checkpoint

  CheckpointLogRecord() {
    this->type = LogRecordType::kCheckpoint;
    this->txn_id = kInvalidTxnId;
    this->prev_lsn = kInvalidLSN;
  }

  std::vector<std::byte> Serialize() const override;
};

// ============================================================================
// LogManager: Write-Ahead Log Implementation
// ============================================================================

// LogManager manages the write-ahead log file
//
// Responsibilities:
// - Append log records with monotonically increasing LSNs
// - Force log to disk on transaction commit (durability)
// - Provide log records for recovery (forward/backward scan)
//
// Usage pattern:
// 1. Begin transaction: AppendBeginRecord(txn_id)
// 2. Modify data: AppendUpdateRecord(txn_id, page_id, offset, old, new)
// 3. Commit: AppendCommitRecord(txn_id), ForceFlush()
// 4. Recovery: ScanForward/ScanBackward to replay/undo operations
//
// Thread safety:
// - Single mutex protects all log operations
// - Caller must coordinate transaction-level locking
class LogManager {
public:
  // Create log manager with specified log file
  // log_file: Path to write-ahead log file
  explicit LogManager(const std::string& log_file);
  ~LogManager();

  // Disable copy/move (manages file handle)
  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;

  // ========== Log Record Operations ==========

  // Append a begin transaction record
  // Returns: LSN of the log record
  LSN AppendBeginRecord(TxnId txn_id, LSN prev_lsn = kInvalidLSN);

  // Append a commit transaction record
  // Returns: LSN of the log record
  LSN AppendCommitRecord(TxnId txn_id, LSN prev_lsn);

  // Append an abort transaction record
  // Returns: LSN of the log record
  LSN AppendAbortRecord(TxnId txn_id, LSN prev_lsn);

  // Append an update record (data modification)
  // txn_id: Transaction performing update
  // prev_lsn: Previous LSN for this transaction
  // page_id: Which page is being modified
  // offset: Offset within page
  // length: Number of bytes changed
  // old_data: Before image
  // new_data: After image
  // Returns: LSN of the log record
  LSN AppendUpdateRecord(TxnId txn_id, LSN prev_lsn, PageId page_id, std::size_t offset,
                         std::size_t length, const std::byte* old_data, const std::byte* new_data);

  // Append a CLR (compensation log record)
  // Used during undo to record the undo operation
  LSN AppendCLRRecord(TxnId txn_id, LSN prev_lsn, PageId page_id, std::size_t offset,
                      std::size_t length, const std::byte* undo_data, LSN undo_next_lsn);

  // Append a checkpoint record
  LSN AppendCheckpointRecord(const std::vector<TxnId>& active_txns);

  // Force log buffer to disk (durability)
  // MUST be called after commit record to ensure durability
  Status ForceFlush();

  // Get current LSN (next LSN to be assigned)
  LSN GetNextLSN() const {
    return next_lsn_;
  }

  // ========== Recovery Operations ==========

  // Scan log forward from given LSN
  // callback: Function called for each log record
  // Returns: Status of scan operation
  Status ScanForward(LSN start_lsn, std::function<void(const LogRecord&)> callback);

  // Scan log backward from given LSN
  // callback: Function called for each log record (newest to oldest)
  // Returns: Status of scan operation
  Status ScanBackward(LSN start_lsn, std::function<void(const LogRecord&)> callback);

  // Get a specific log record by LSN
  std::unique_ptr<LogRecord> GetLogRecord(LSN lsn);

private:
  // Append a log record to the log file
  LSN AppendLogRecord(std::unique_ptr<LogRecord> record);

  std::string log_file_path_; // Path to log file
  std::fstream log_stream_;   // File stream for log
  LSN next_lsn_;              // Next LSN to assign
  std::size_t log_offset_;    // Current offset in log file

  mutable std::mutex latch_; // Protects log operations
};

} // namespace core_engine
