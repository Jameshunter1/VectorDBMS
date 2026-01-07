#include <core_engine/lsm/lsm_tree.hpp>

// Every line is commented (as requested) for the new LSM module.

#include <filesystem>                      // Filesystem helpers.

namespace core_engine {

LsmTree::LsmTree() : wal_(std::filesystem::path{}) {}           // Start with an empty WAL path.

Status LsmTree::Open(std::filesystem::path db_dir) {            // Legacy API - uses embedded config.
  return Open(DatabaseConfig::Embedded(std::move(db_dir)));     // Delegate to config-based Open.
}                                                               // End legacy Open.

Status LsmTree::Open(const DatabaseConfig& config) {            // Open the LSM storage with configuration.
  config_ = config;                                             // Store configuration.
  
  // Initialize all directories (data, WAL, levels).
  if (!config_.Initialize()) {                                  // If directory creation failed...
    return Status::IoError("Failed to initialize database directories"); // ...report error.
  }                                                             // End initialization check.
  
  db_dir_ = config_.root_dir;                                   // Store root for compatibility.
  
  /**
   * MANIFEST RECOVERY
   * 
   * The manifest tells us which SSTables exist and should be loaded.
   * We must open it BEFORE replaying the WAL, because the WAL only contains
   * data that wasn't flushed yet. The manifest tells us about flushed data.
   */
  if (!manifest_.Open(config_.GetManifestPath())) {             // Open manifest in data directory.
    return Status::IoError("Failed to open manifest");          // Propagate error.
  }                                                             // End manifest open check.
  
  auto status = RecoverFromManifest();                          // Load SSTables listed in manifest.
  if (!status.ok()) {                                           // If recovery failed...
    return status;                                               // ...propagate error.
  }                                                             // End manifest recovery check.
  
  wal_ = Wal(config_.GetWalPath());                             // WAL in configured directory (may be separate disk).

  status = wal_.OpenOrCreate();                                 // Ensure WAL exists.
  if (!status.ok()) {                                           // If WAL open failed...
    return status;                                               // ...bubble up error.
  }                                                             // End WAL error check.

  status = wal_.Replay([this](WalRecordType type, std::string key, std::string value) { // Replay WAL to rebuild MemTable.
    if (type == WalRecordType::kPut) {                           // If record is a Put...
      memtable_.Put(std::move(key), std::move(value));           // ...apply it to MemTable.
      return Status::Ok();                                       // Success.
    } else if (type == WalRecordType::kDelete) {                 // If record is a Delete...
      memtable_.Delete(std::move(key));                          // ...mark as deleted in MemTable.
      return Status::Ok();                                       // Success.
    }                                                            // End record-type check.
    return Status::Corruption("Unknown WAL record type");      // Anything else is corruption.
  });                                                            // End replay.
  if (!status.ok()) {                                            // If replay failed...
    return status;                                               // ...bubble up error.
  }                                                              // End replay error check.

  is_open_ = true;                                              // Mark open.
  return Status::Ok();                                          // Success.
}                                                               // End Open.

Status LsmTree::Put(std::string key, std::string value) {       // Put into WAL + MemTable.
  if (!is_open_) {                                              // Prevent use before open.
    return Status::Internal("LSMTree is not open");             // Return an internal state error.
  }                                                             // End state check.

  auto status = wal_.AppendPut(key, value);                      // Append to WAL first (durability ordering).
  if (!status.ok()) {                                           // If WAL append failed...
    return status;                                               // ...do not mutate MemTable.
  }                                                             // End WAL error check.

  memtable_.Put(std::move(key), std::move(value));              // Apply to MemTable.
  
  status = MaybeFlushMemTable();                                // Check if MemTable needs flushing.
  if (!status.ok()) {                                           // If flush failed...
    return status;                                               // ...propagate error.
  }                                                             // End flush check.
  
  return Status::Ok();                                          // Success.
}                                                               // End Put.

Status LsmTree::Delete(std::string key) {                       // Delete a key (write tombstone).
  /**
   * DELETE IMPLEMENTATION
   * 
   * Deletes in LSM-trees work by writing a "tombstone" marker.
   * This is much faster than finding and removing the key from disk files.
   * 
   * Flow:
   * 1. Write tombstone to WAL (durability)
   * 2. Write tombstone to MemTable (makes delete visible immediately)
   * 3. During Get(), tombstones return nullopt (key not found)
   * 4. During flush, tombstones are written to SSTables
   * 5. During compaction, tombstones can be removed once they shadow all older values
   */
  if (!is_open_) {                                              // Prevent use before open.
    return Status::Internal("LSMTree is not open");             // Return an internal state error.
  }                                                             // End state check.

  auto status = wal_.AppendDelete(key);                         // Append delete to WAL first (durability).
  if (!status.ok()) {                                           // If WAL append failed...
    return status;                                               // ...do not mutate MemTable.
  }                                                             // End WAL error check.

  memtable_.Delete(std::move(key));                             // Mark key as deleted in MemTable.
  
  status = MaybeFlushMemTable();                                // Check if MemTable needs flushing.
  if (!status.ok()) {                                           // If flush failed...
    return status;                                               // ...propagate error.
  }                                                             // End flush check.
  
  return Status::Ok();                                          // Success.
}                                                               // End Delete.

std::optional<std::string> LsmTree::Get(std::string key) {      // Get from MemTable + SSTables.
  if (!is_open_) {                                              // Prevent use before open.
    return std::nullopt;                                        // Keep Get non-throwing; caller can detect missing.
  }                                                             // End state check.

  auto result = memtable_.Get(key);                             // Check MemTable first (newest data).
  if (result.has_value()) {                                     // If found...
    return result;                                              // ...return it.
  }                                                             // End MemTable check.
  
  // Search all SSTables across all levels (L0 first, then L1, L2, etc.)
  auto all_sstables = leveled_lsm_.GetAllSSTables();           // Get SSTables in correct order.
  for (auto* sstable : all_sstables) {                          // Search each SSTable.
    result = sstable->Get(key);                                 // Binary search in SSTable.
    if (result.has_value()) {                                   // If found...
      return result;                                            // ...return it.
    }                                                           // End SSTable check.
  }                                                             // End SSTable loop.

  return std::nullopt;                                          // Key not found.
}                                                               // End Get.

Status LsmTree::MaybeFlushMemTable() {                          // Conditionally flush MemTable.
  if (memtable_.ApproximateSizeBytes() >= kMemTableFlushThresholdBytes) { // If over threshold...
    return FlushMemTable();                                     // ...flush to SSTable.
  }                                                             // End threshold check.
  return Status::Ok();                                          // No flush needed.
}                                                               // End MaybeFlushMemTable.

Status LsmTree::FlushMemTable() {                               // Flush MemTable to a new SSTable.
  if (memtable_.Size() == 0) {                                  // If MemTable is empty...
    return Status::Ok();                                        // ...nothing to flush.
  }                                                             // End empty check.
  
  const uint64_t new_sstable_id = next_sstable_id_++;           // Get ID for new SSTable.
  const auto sstable_path = GetSSTablePath(new_sstable_id, 0);  // New SSTables go to L0.
  
  SSTableWriter writer(sstable_path);                           // Create SSTable writer.
  auto status = writer.Open();                                  // Open for writing.
  if (!status.ok()) {                                           // If open failed...
    return status;                                              // ...propagate error.
  }                                                             // End open check.
  
  auto entries = memtable_.GetAllSorted();                      // Get all entries in sorted order.
  for (const auto& [key, value] : entries) {                    // Write each entry...
    status = writer.Add(key, value);                            // ...to the SSTable.
    if (!status.ok()) {                                         // If add failed...
      return status;                                            // ...propagate error.
    }                                                           // End add check.
  }                                                             // End entries loop.
  
  status = writer.Finish();                                     // Finalize and close SSTable.
  if (!status.ok()) {                                           // If finish failed...
    return status;                                              // ...propagate error.
  }                                                             // End finish check.
  
  /**
   * MANIFEST UPDATE
   * 
   * After creating the SSTable file, we must record it in the manifest.
   * This ensures that if we crash and restart, we know this SSTable exists.
   */
  manifest_.AddSSTable(new_sstable_id);                         // Record new SSTable in manifest.
  
  auto reader = std::make_unique<SSTableReader>(sstable_path);  // Create reader for this SSTable.
  status = reader->Open();                                      // Open and parse SSTable.
  if (!status.ok()) {                                           // If open failed...
    return status;                                              // ...propagate error.
  }                                                             // End reader open check.
  
  // Add SSTable to L0 (Level 0 always receives fresh flushes).
  leveled_lsm_.AddL0SSTable(std::move(reader));                 // Add to multi-level structure.
  memtable_.Clear();                                            // Clear MemTable.
  
  /**
   * AUTOMATIC LEVELED COMPACTION WITH MANIFEST COORDINATION
   * 
   * After flushing to L0, check if any level needs compaction.
   * If compaction occurs, update the manifest to track which SSTables
   * were added and which were deleted.
   * 
   * This is the critical fix that enables reliable auto-compaction:
   * the LeveledLSM returns which IDs changed, and we update the manifest.
   * 
   * We pass config_.data_dir so Level can organize files in level_N/ subdirectories.
   */
  auto compaction_result = leveled_lsm_.MaybeCompact(config_.data_dir, next_sstable_id_);
  if (compaction_result.performed) {
    // Update manifest with compaction changes
    manifest_.RemoveSSTables(compaction_result.removed_ids);
    for (uint64_t added_id : compaction_result.added_ids) {
      manifest_.AddSSTable(added_id);
    }
  }
  
  return Status::Ok();                                          // Flush succeeded.
}                                                               // End FlushMemTable.

std::size_t LsmTree::GetMemTableSizeBytes() const {             // Return MemTable memory usage.
  return memtable_.ApproximateSizeBytes();                      // Delegate to MemTable.
}                                                               // End GetMemTableSizeBytes.

std::size_t LsmTree::GetMemTableEntryCount() const {            // Return MemTable entry count.
  return memtable_.Size();                                      // Delegate to MemTable.
}                                                               // End GetMemTableEntryCount.

std::size_t LsmTree::GetSSTableCount() const {                  // Return number of SSTables.
  auto all_sstables = leveled_lsm_.GetAllSSTables();            // Get all SSTables across all levels.
  return all_sstables.size();                                   // Return total count.
}                                                               // End GetSSTableCount.

std::size_t LsmTree::GetBloomFilterChecks() const {             // Total Bloom filter checks across all SSTables.
  std::size_t total = 0;
  auto all_sstables = leveled_lsm_.GetAllSSTables();
  for (const auto* sstable : all_sstables) {
    total += sstable->GetBloomFilterChecks();
  }
  return total;
}

std::size_t LsmTree::GetBloomFilterHits() const {               // Total Bloom filter hits (avoided reads).
  std::size_t total = 0;
  auto all_sstables = leveled_lsm_.GetAllSSTables();
  for (const auto* sstable : all_sstables) {
    total += sstable->GetBloomFilterHits();
  }
  return total;
}

std::size_t LsmTree::GetBloomFilterFalsePositives() const {     // Total false positives.
  std::size_t total = 0;
  auto all_sstables = leveled_lsm_.GetAllSSTables();
  for (const auto* sstable : all_sstables) {
    total += sstable->GetBloomFilterFalsePositives();
  }
  return total;
}

std::vector<std::pair<std::string, std::string>> LsmTree::GetAllEntries() const {
  /**
   * GET ALL ENTRIES
   * 
   * Returns all key-value pairs in the database, newest values first.
   * This is useful for debugging and for displaying data in the UI.
   * 
   * Algorithm:
   * 1. Start with MemTable entries (newest)
   * 2. Add SSTable entries (older), skipping keys we've already seen
   * 3. Skip tombstones (deleted keys)
   */
  std::map<std::string, std::string> all_entries;
  
  // Add SSTable entries first (oldest to newest, map will overwrite)
  auto all_sstables = leveled_lsm_.GetAllSSTables();            // Get all SSTables.
  for (auto it = all_sstables.rbegin(); it != all_sstables.rend(); ++it) {
    auto* sstable = *it;
    auto entries = sstable->GetAllSorted();
    for (const auto& [key, value] : entries) {
      all_entries[key] = value;
    }
  }
  
  // Add MemTable entries last (newest, will overwrite SSTable values)
  auto memtable_entries = memtable_.GetAllSorted();
  for (const auto& [key, value] : memtable_entries) {
    all_entries[key] = value;
  }
  
  // Convert to vector, filtering out tombstones
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& [key, value] : all_entries) {
    if (value != MemTable::kTombstoneValue) {  // Skip deleted keys
      result.emplace_back(key, value);
    }
  }
  
  return result;
}

Status LsmTree::RecoverFromManifest() {                         // Load SSTables listed in manifest.
  /**
   * MANIFEST RECOVERY
   * 
   * The manifest is a log of ADD/REMOVE operations.
   * We replay it to discover which SSTables should be loaded.
   * 
   * Example manifest:
   *   ADD 0
   *   ADD 1
   *   ADD 2
   *   REMOVE 0
   *   REMOVE 1
   *   ADD 3
   * 
   * Result: Load sstable_2.sst and sstable_3.sst
   */
  
  std::vector<uint64_t> active_ids;                             // IDs of SSTables to load.
  if (!manifest_.GetActiveSSTables(active_ids)) {               // Replay manifest.
    return Status::Corruption("Failed to read manifest");       // Propagate error.
  }                                                             // End manifest replay.
  
  // Load each SSTable from disk and add to L0.
  // TODO: Manifest should track which level each SSTable belongs to for proper recovery.
  // For now, search all level directories to find the file.
  for (uint64_t id : active_ids) {                              // For each active SSTable ID...
    std::filesystem::path sstable_path;
    bool found = false;
    
    // Try each level directory (L0 through L5 should be enough)
    for (int level = 0; level <= 5 && !found; ++level) {
      auto candidate_path = GetSSTablePath(id, level);
      if (std::filesystem::exists(candidate_path)) {
        sstable_path = candidate_path;
        found = true;
      }
    }
    
    // Fall back to legacy flat structure if not found in level directories
    if (!found) {
      auto legacy_path = db_dir_ / ("sstable_" + std::to_string(id) + ".sst");
      if (std::filesystem::exists(legacy_path)) {
        sstable_path = legacy_path;
        found = true;
      }
    }
    
    // Check if file exists
    if (!found) {                                               // If file missing...
      // This is a consistency error — manifest says it exists, but it doesn't.
      return Status::Corruption("SSTable file missing (ID " + std::to_string(id) + "), searched all levels");
    }                                                           // End exists check.
    
    auto reader = std::make_unique<SSTableReader>(sstable_path); // Create reader.
    auto status = reader->Open();                               // Open and parse SSTable.
    if (!status.ok()) {                                         // If open failed...
      return status;                                            // ...propagate error.
    }                                                           // End open check.
    
    // Add to L0 (for now, all recovered SSTables go to L0).
    leveled_lsm_.AddL0SSTable(std::move(reader));               // Add to leveled structure.
    
    // Update next_sstable_id_ to avoid ID collisions.
    // If we load sstable_5.sst, the next new SSTable should be sstable_6.sst.
    if (id >= next_sstable_id_) {                               // If this ID is >= current counter...
      next_sstable_id_ = id + 1;                                // ...bump counter past it.
    }                                                           // End ID update.
  }                                                             // End SSTable loading loop.
  
  return Status::Ok();                                          // Recovery succeeded.
}                                                               // End RecoverFromManifest.

std::filesystem::path LsmTree::GetSSTablePath(uint64_t sstable_id, int level) const {
  return config_.GetSSTablePath(sstable_id, level);             // Use config to determine path.
}                                                               // End GetSSTablePath.

}  // namespace core_engine
