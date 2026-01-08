#include <core_engine/engine.hpp>

// core_engine/engine.cpp
//
// Purpose:
// - Implement the Engine façade for Year 1 Q4 (Write-Ahead Logging).
// - Key-value storage using BufferPoolManager with LRU-K eviction (Q3).
// - LogManager provides WAL for durability and recovery (Q4).
// - DiskManager provides persistent storage backend.

#include <core_engine/common/logger.hpp>

#include <chrono>
#include <filesystem>
#include <sstream>
#include <utility>

namespace core_engine {

Engine::Engine() = default;

Engine::~Engine() {
  // Explicit destruction order to prevent use-after-free:
  // 1. Destroy buffer pool first (flushes pages while disk manager is still valid)
  // 2. Then log manager
  // 3. Finally disk manager
  Log(LogLevel::kDebug, "Engine::~Engine() starting");
  buffer_pool_manager_.reset();
  Log(LogLevel::kDebug, "BufferPoolManager reset complete");
  log_manager_.reset();
  Log(LogLevel::kDebug, "LogManager reset complete");
  disk_manager_.reset();
  Log(LogLevel::kDebug, "DiskManager reset complete");
  vector_index_.reset();
  Log(LogLevel::kDebug, "Engine::~Engine() complete");
}

Status Engine::Open(std::filesystem::path db_path) {
  return Open(DatabaseConfig::Embedded(std::move(db_path)));
}

Status Engine::Open(const DatabaseConfig& config) {
  if (is_open_) {
    return Status::AlreadyExists("Engine already open");
  }

  config_ = config;

  // Create database directory if it doesn't exist
  if (!std::filesystem::exists(config_.data_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(config_.data_dir, ec);
    if (ec) {
      return Status::IoError("Failed to create data directory: " + ec.message());
    }
  }

  // Create DiskManager for page I/O
  auto db_file = config_.data_dir / "pages.db";
  disk_manager_ = std::make_unique<DiskManager>(db_file);

  auto status = disk_manager_->Open();
  if (!status.ok()) {
    Log(LogLevel::kError, "Failed to open DiskManager: " + status.ToString());
    return status;
  }

  // Create BufferPoolManager for page caching (Year 1 Q3 - LRU-K)
  buffer_pool_manager_ =
      std::make_unique<BufferPoolManager>(config_.buffer_pool_size, disk_manager_.get());
  Log(LogLevel::kDebug, "BufferPoolManager created (pool_size=" +
                            std::to_string(config_.buffer_pool_size) + " pages, LRU-K eviction)");

  // Create LogManager for write-ahead logging (Year 1 Q4 - WAL)
  auto log_file = config_.data_dir / "wal.log";
  log_manager_ = std::make_unique<LogManager>(log_file.string());
  Log(LogLevel::kDebug, "LogManager created (log_file=" + log_file.string() + ")");

  // Initialize vector index if enabled
  if (config_.enable_vector_index) {
    vector::HNSWIndex::Params hnsw_params;
    hnsw_params.dimension = config_.vector_dimension;

    // Convert config metric to vector metric
    switch (config_.vector_metric) {
    case DatabaseConfig::VectorDistanceMetric::kCosine:
      hnsw_params.metric = vector::DistanceMetric::kCosine;
      break;
    case DatabaseConfig::VectorDistanceMetric::kEuclidean:
      hnsw_params.metric = vector::DistanceMetric::kEuclidean;
      break;
    case DatabaseConfig::VectorDistanceMetric::kDotProduct:
      hnsw_params.metric = vector::DistanceMetric::kDotProduct;
      break;
    case DatabaseConfig::VectorDistanceMetric::kManhattan:
      hnsw_params.metric = vector::DistanceMetric::kManhattan;
      break;
    }

    hnsw_params.M = config_.hnsw_params.M;
    hnsw_params.ef_construction = config_.hnsw_params.ef_construction;
    hnsw_params.ef_search = config_.hnsw_params.ef_search;

    vector_index_ = std::make_unique<vector::HNSWIndex>(hnsw_params);
    Log(LogLevel::kInfo,
        "Vector index initialized (dimension=" + std::to_string(config_.vector_dimension) + ")");
  }

  // Rebuild in-memory key-to-page index by scanning all existing pages
  // This is necessary after restart to restore O(1) key lookup performance
  Log(LogLevel::kDebug, "Rebuilding key index...");
  PageId num_pages = disk_manager_->GetNumPages();
  std::size_t keys_indexed = 0;

  for (PageId page_id = 1; page_id <= num_pages; ++page_id) {
    auto page = buffer_pool_manager_->FetchPage(page_id);
    if (!page) {
      // DEBUG: Expected when reusing database directories (benchmarks, tests)
      Log(LogLevel::kDebug,
          "Failed to fetch page " + std::to_string(page_id) + " during index rebuild");
      continue;
    }

    // Parse key from page
    const char* data = page->GetData();
    std::size_t offset = 0;

    // Read key_size
    if (offset + sizeof(uint32_t) > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    uint32_t key_size;
    std::memcpy(&key_size, data + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Read key
    if (key_size == 0 || offset + key_size > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    std::string stored_key(data + offset, key_size);
    offset += key_size;

    // Read value_size to check for tombstone
    if (offset + sizeof(uint32_t) > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    uint32_t value_size;
    std::memcpy(&value_size, data + offset, sizeof(uint32_t));

    // Skip tombstones (deleted entries)
    if (value_size == UINT32_MAX) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    // Add to index
    key_to_page_[stored_key] = page_id;
    keys_indexed++;

    buffer_pool_manager_->UnpinPage(page_id, false);
  }

  Log(LogLevel::kDebug, "Index rebuilt: " + std::to_string(keys_indexed) + " keys indexed from " +
                            std::to_string(num_pages) + " pages");

  is_open_ = true;
  Log(LogLevel::kDebug, "Engine opened (Year 1 Q4 - Write-Ahead Logging + LRU-K Buffer Pool)");
  return Status::Ok();
}

Status Engine::Put(std::string key, std::string value) {
  if (!is_open_) {
    return Status::Internal("Engine is not open");
  }

  // Year 1 Q4: Write-Ahead Logging + BufferPoolManager
  // v1.5: Group commit optimization - batch operations share transactions
  // 1. Start transaction and log the update
  // 2. Write to buffered page
  // 3. Commit transaction (deferred in batch mode)

  auto start = std::chrono::high_resolution_clock::now();

  // Use batch transaction if in batch mode, otherwise create new transaction
  TxnId txn_id;
  LSN begin_lsn;

  if (batch_mode_ && batch_txn_id_ > 0) {
    // Reuse existing batch transaction
    txn_id = batch_txn_id_;
    begin_lsn = batch_begin_lsn_;
  } else {
    // Create new transaction
    txn_id = next_txn_id_++;
    begin_lsn = log_manager_->AppendBeginRecord(txn_id);
  }

  // Check in-memory index if key already exists (O(1) vs O(N) linear scan)
  PageId existing_page_id = 0;
  auto it = key_to_page_.find(key);
  if (it != key_to_page_.end()) {
    existing_page_id = it->second;
  }

  // Allocate or reuse page
  PageId page_id_out;
  Page* page;
  if (existing_page_id > 0) {
    page_id_out = existing_page_id;
    page = buffer_pool_manager_->FetchPage(page_id_out);
    if (!page) {
      log_manager_->AppendAbortRecord(txn_id, begin_lsn);
      log_manager_->ForceFlush();
      return Status::Internal("Failed to fetch existing page from buffer pool");
    }
  } else {
    page = buffer_pool_manager_->NewPage(&page_id_out);
    if (!page) {
      log_manager_->AppendAbortRecord(txn_id, begin_lsn);
      log_manager_->ForceFlush();
      return Status::Internal("Failed to allocate page from buffer pool");
    }
  }

  // Write key-value to page (simplified format for Q3/Q4)
  // Format: [key_size(4 bytes)][key][value_size(4 bytes)][value]
  char* data = page->GetData();
  std::size_t data_offset = 0;

  uint32_t key_size = static_cast<uint32_t>(key.size());
  std::memcpy(data + data_offset, &key_size, sizeof(uint32_t));
  data_offset += sizeof(uint32_t);
  std::memcpy(data + data_offset, key.data(), key.size());
  data_offset += key.size();

  uint32_t value_size = static_cast<uint32_t>(value.size());
  std::memcpy(data + data_offset, &value_size, sizeof(uint32_t));
  data_offset += sizeof(uint32_t);

  std::size_t value_data_start = data_offset;
  std::memcpy(data + data_offset, value.data(), value.size());

  // Log the update (WAL - write log before modifying page)
  LSN update_lsn = log_manager_->AppendUpdateRecord(
      txn_id, begin_lsn, page_id_out, value_data_start, value.size(),
      nullptr, // No old data (this is an insert)
      reinterpret_cast<const std::byte*>(value.data()));

  // Update in-memory index
  key_to_page_[key] = page_id_out;

  // Mark page as dirty and unpin
  buffer_pool_manager_->UnpinPage(page->GetPageId(), true);

  // Log commit and force to disk (deferred in batch mode)
  LSN commit_lsn;
  if (!batch_mode_) {
    // Normal mode: commit immediately with fsync
    commit_lsn = log_manager_->AppendCommitRecord(txn_id, update_lsn);
    log_manager_->ForceFlush();
  } else {
    // Batch mode: defer commit until EndBatch()
    commit_lsn = update_lsn; // No commit record yet
  }

  auto end = std::chrono::high_resolution_clock::now();
  total_put_time_us_ += std::chrono::duration<double, std::micro>(end - start).count();
  ++total_puts_;

  // Verbose debug logging disabled for performance (floods logs in benchmarks)
  // Log(LogLevel::kDebug,
  //     "Put: " + key + " = " + value + " (page_id=" + std::to_string(page->GetPageId()) +
  //         ", txn=" + std::to_string(txn_id) + ", lsn=" + std::to_string(commit_lsn) + ")");
  return Status::Ok();
}

std::optional<std::string> Engine::Get(std::string key) {
  if (!is_open_) {
    return std::nullopt;
  }

  // Year 1 Q2: Use BufferPoolManager for cached page I/O
  // Simple linear scan through pages for now
  // Future Q3: Proper B-tree indexing for O(log n) lookups

  auto start = std::chrono::high_resolution_clock::now();

  // Use in-memory index for O(1) lookup instead of O(N) scan
  auto it = key_to_page_.find(key);
  if (it == key_to_page_.end()) {
    return std::nullopt; // Key not found
  }

  PageId page_id = it->second;
  auto page = buffer_pool_manager_->FetchPage(page_id);
  if (!page) {
    auto end = std::chrono::high_resolution_clock::now();
    total_get_time_us_ += std::chrono::duration<double, std::micro>(end - start).count();
    ++total_gets_;
    return std::nullopt;
  }

  // Parse key-value from page
  const char* data = page->GetData();
  std::size_t offset = 0;

  if (offset + sizeof(uint32_t) > kPageSize) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    return std::nullopt;
  }

  uint32_t key_size;
  std::memcpy(&key_size, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  if (offset + key_size > kPageSize) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    return std::nullopt;
  }

  offset += key_size; // Skip the key (we already know it matches)

  // Check if it's deleted (tombstone)
  if (offset + sizeof(uint32_t) > kPageSize) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    return std::nullopt;
  }

  uint32_t value_size;
  std::memcpy(&value_size, data + offset, sizeof(uint32_t));

  // Check for tombstone marker
  if (value_size == UINT32_MAX) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    auto end = std::chrono::high_resolution_clock::now();
    total_get_time_us_ += std::chrono::duration<double, std::micro>(end - start).count();
    ++total_gets_;
    Log(LogLevel::kDebug,
        "Get: " + key + " (found tombstone on page_id=" + std::to_string(page_id) + ")");
    return std::nullopt; // Key was deleted
  }

  offset += sizeof(uint32_t);

  if (offset + value_size > kPageSize) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    return std::nullopt;
  }

  std::string value(data + offset, value_size);
  buffer_pool_manager_->UnpinPage(page_id, false);

  auto end = std::chrono::high_resolution_clock::now();
  total_get_time_us_ += std::chrono::duration<double, std::micro>(end - start).count();
  ++total_gets_;

  // Verbose debug logging disabled for performance
  // Log(LogLevel::kDebug, "Get: " + key + " (found on page_id=" + std::to_string(page_id) + ")");
  return value;
}

Status Engine::Delete(std::string key) {
  if (!is_open_) {
    return Status::Internal("Engine is not open");
  }

  // Use in-memory index for O(1) lookup
  auto it = key_to_page_.find(key);
  if (it == key_to_page_.end()) {
    Log(LogLevel::kDebug, "Delete: " + key + " (key not found)");
    return Status::Ok(); // Deleting non-existent key is OK
  }

  PageId page_id = it->second;
  auto page = buffer_pool_manager_->FetchPage(page_id);
  if (!page) {
    return Status::Internal("Failed to fetch page for deletion");
  }

  // Parse key from page to find offset
  const char* data = page->GetData();
  std::size_t offset = 0;

  if (offset + sizeof(uint32_t) > kPageSize) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    return Status::Corruption("Page corrupted");
  }

  uint32_t key_size;
  std::memcpy(&key_size, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  if (offset + key_size > kPageSize) {
    buffer_pool_manager_->UnpinPage(page_id, false);
    return Status::Corruption("Page corrupted");
  }

  offset += key_size; // Skip to value_size field

  // Mark as deleted by setting value_size to UINT32_MAX (tombstone)
  uint32_t tombstone = UINT32_MAX;
  char* write_data = page->GetData();
  std::memcpy(write_data + offset, &tombstone, sizeof(uint32_t));

  page->MarkDirty();
  page->UpdateChecksum();
  buffer_pool_manager_->UnpinPage(page_id, true);

  // Remove from index (key still on disk as tombstone, but not in memory index)
  key_to_page_.erase(it);

  Log(LogLevel::kDebug,
      "Delete: " + key + " (marked with tombstone on page_id=" + std::to_string(page_id) + ")");
  return Status::Ok();
}

Status Engine::BatchWrite(const std::vector<BatchOperation>& operations) {
  // Use group commit optimization for batch operations
  BeginBatch();

  for (const auto& op : operations) {
    Status status = Status::Ok();
    if (op.type == BatchOperation::Type::PUT) {
      status = Put(op.key, op.value);
    } else {
      status = Delete(op.key);
    }

    if (!status.ok()) {
      EndBatch(); // Flush partial batch on error
      return status;
    }
  }

  return EndBatch(); // Single commit for all operations
}

std::vector<std::optional<std::string>> Engine::BatchGet(const std::vector<std::string>& keys) {
  std::vector<std::optional<std::string>> results;
  results.reserve(keys.size());

  for (const auto& key : keys) {
    results.push_back(Get(key));
  }

  return results;
}

std::vector<std::pair<std::string, std::string>>
Engine::Scan(const std::string& start_key, const std::string& end_key, const ScanOptions& options) {
  std::vector<std::pair<std::string, std::string>> results;

  if (!is_open_) {
    return results;
  }

  // Collect all key-value pairs from pages
  std::vector<std::pair<std::string, std::string>> all_kvs;

  for (PageId page_id = 1; page_id <= disk_manager_->GetNumPages(); ++page_id) {
    auto page = buffer_pool_manager_->FetchPage(page_id);
    if (!page)
      continue;

    const char* data = page->GetData();
    std::size_t offset = 0;

    // Parse key-value
    if (offset + sizeof(uint32_t) > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    uint32_t key_size;
    std::memcpy(&key_size, data + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (offset + key_size > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    std::string key(data + offset, key_size);
    offset += key_size;

    if (offset + sizeof(uint32_t) > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    uint32_t value_size;
    std::memcpy(&value_size, data + offset, sizeof(uint32_t));

    // Skip tombstones
    if (value_size == UINT32_MAX) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    offset += sizeof(uint32_t);

    if (offset + value_size > kPageSize) {
      buffer_pool_manager_->UnpinPage(page_id, false);
      continue;
    }

    std::string value(data + offset, value_size);

    // Check if key is in range [start_key, end_key)
    if (key >= start_key && key < end_key) {
      if (options.keys_only) {
        all_kvs.emplace_back(key, "");
      } else {
        all_kvs.emplace_back(key, value);
      }
    }

    buffer_pool_manager_->UnpinPage(page_id, false);
  }

  // Sort by key
  std::sort(all_kvs.begin(), all_kvs.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  // Apply reverse if needed
  if (options.reverse) {
    std::reverse(all_kvs.begin(), all_kvs.end());
  }

  // Apply limit
  if (options.limit > 0 && all_kvs.size() > options.limit) {
    all_kvs.resize(options.limit);
  }

  return all_kvs;
}

Status Engine::Execute(std::string_view statement) {
  (void)statement; // Suppress unused parameter warning
  return Status::Unimplemented("SQL execution not implemented (Year 1 Q1 focuses on page layer)");
}

Engine::Stats Engine::GetStats() const {
  Stats stats{};

  if (disk_manager_) {
    auto disk_stats = disk_manager_->GetStats();
    stats.total_pages = disk_manager_->GetNumPages();
    stats.total_reads = disk_stats.total_reads;
    stats.total_writes = disk_stats.total_writes;
    stats.checksum_failures = disk_stats.checksum_failures;
  }

  stats.total_gets = total_gets_;
  stats.total_puts = total_puts_;
  stats.avg_get_time_us = (total_gets_ > 0) ? (total_get_time_us_ / total_gets_) : 0.0;
  stats.avg_put_time_us = (total_puts_ > 0) ? (total_put_time_us_ / total_puts_) : 0.0;
  stats.total_entries = key_to_page_.size();

  return stats;
}

std::vector<std::pair<std::string, std::string>> Engine::GetAllEntries() const {
  std::vector<std::pair<std::string, std::string>> entries;

  if (!is_open_ || !buffer_pool_manager_) {
    return entries;
  }

  // Iterate through all pages in the index
  // Note: We need to use const_cast here since Get() is non-const
  // This is safe because Get() only modifies cache state, not logical database state
  Engine* mutable_this = const_cast<Engine*>(this);

  for (const auto& [key, page_id] : key_to_page_) {
    auto value_opt = mutable_this->Get(key);
    if (value_opt.has_value()) {
      entries.emplace_back(key, value_opt.value());
    }
  }

  return entries;
}

// ====== Vector Database Operations ======

Status Engine::PutVector(const std::string& key, const vector::Vector& vec) {
  if (!vector_index_) {
    return Status::Internal("Vector index not enabled");
  }

  if (vec.dimension() != config_.vector_dimension) {
    return Status::InvalidArgument("Vector dimension mismatch");
  }

  // Insert into HNSW index
  auto status = vector_index_->Insert(key, vec);
  if (!status.ok()) {
    return status;
  }

  // Store serialized vector (Year 1 Q1: simplified)
  std::string serialized = vec.Serialize();
  return Put(key, serialized);
}

std::vector<Engine::VectorSearchResult> Engine::SearchSimilar(const vector::Vector& query,
                                                              std::size_t k, bool include_vectors) {
  std::vector<VectorSearchResult> results;

  if (!vector_index_) {
    return results;
  }

  auto hnsw_results = vector_index_->Search(query, k);

  for (const auto& result : hnsw_results) {
    VectorSearchResult vr;
    vr.key = result.key;
    vr.distance = result.distance;

    if (include_vectors) {
      auto vec_opt = GetVector(result.key);
      if (vec_opt.has_value()) {
        vr.vector = *vec_opt;
      }
    }

    results.push_back(std::move(vr));
  }

  return results;
}

std::optional<vector::Vector> Engine::GetVector(const std::string& key) {
  auto serialized_opt = Get(key);
  if (!serialized_opt.has_value()) {
    return std::nullopt;
  }

  return vector::Vector::Deserialize(*serialized_opt);
}

Status Engine::BatchPutVectors(const std::vector<std::pair<std::string, vector::Vector>>& vectors) {
  for (const auto& [key, vec] : vectors) {
    auto status = PutVector(key, vec);
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

std::vector<std::optional<vector::Vector>>
Engine::BatchGetVectors(const std::vector<std::string>& keys) {
  std::vector<std::optional<vector::Vector>> results;
  results.reserve(keys.size());

  for (const auto& key : keys) {
    results.push_back(GetVector(key));
  }

  return results;
}

Engine::VectorStats Engine::GetVectorStats() const {
  VectorStats stats;

  if (!vector_index_) {
    return stats;
  }

  stats.index_enabled = true;
  stats.dimension = vector_index_->dimension();
  stats.metric = vector::ToString(vector_index_->metric());

  auto hnsw_stats = vector_index_->GetStats();
  stats.num_vectors = hnsw_stats.num_vectors;
  stats.num_layers = hnsw_stats.num_layers;
  stats.avg_connections_per_node = hnsw_stats.avg_connections_per_node;

  return stats;
}

// ============================================================================
// Group Commit Implementation (v1.5)
// ============================================================================

void Engine::BeginBatch() {
  if (batch_mode_) {
    // Already in batch mode, ignore
    return;
  }

  batch_mode_ = true;
  batch_txn_id_ = next_txn_id_++;
  batch_begin_lsn_ = log_manager_->AppendBeginRecord(batch_txn_id_);

  Log(LogLevel::kDebug, "Batch started (txn=" + std::to_string(batch_txn_id_) +
                            ", lsn=" + std::to_string(batch_begin_lsn_) + ")");
}

Status Engine::EndBatch() {
  if (!batch_mode_) {
    return Status::Ok(); // Not in batch mode, nothing to do
  }

  // Commit the batch transaction
  LSN commit_lsn = log_manager_->AppendCommitRecord(batch_txn_id_, batch_begin_lsn_);

  // Force log to disk (this is the expensive operation we batched)
  auto status = log_manager_->ForceFlush();

  // Reset batch state
  batch_mode_ = false;
  batch_txn_id_ = 0;
  batch_begin_lsn_ = 0;

  Log(LogLevel::kDebug, "Batch committed (lsn=" + std::to_string(commit_lsn) + ")");

  return status;
}

Status Engine::Flush() {
  if (!batch_mode_) {
    // Not in batch mode, nothing buffered to flush
    return Status::Ok();
  }

  // Flush log without ending batch
  return log_manager_->ForceFlush();
}

} // namespace core_engine
