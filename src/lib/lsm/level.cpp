#include "core_engine/lsm/level.hpp"
#include "core_engine/lsm/memtable.hpp"  // For tombstone constant

#include <algorithm>
#include <fstream>
#include <map>
#include <unordered_set>

namespace core_engine {

// ============================================================================
// Level Implementation
// ============================================================================

Level::Level(int level_num) : level_num_(level_num) {}

void Level::AddSSTable(std::unique_ptr<SSTableReader> sstable) {
    /**
     * ADDING SSTABLES TO LEVELS
     * 
     * When we add an SSTable, we track its size to know when the level
     * needs compaction. Each level has a size limit (10x the previous level).
     */
    if (!sstable) {
        return;  // Null check
    }
    
    // Get the file size to track total level size
    std::error_code ec;
    auto file_size = std::filesystem::file_size(sstable->file_path(), ec);
    if (!ec) {
        total_size_ += file_size;
    }
    
    sstables_.push_back(std::move(sstable));
}

void Level::RemoveSSTables(const std::vector<uint64_t>& ids) {
    /**
     * REMOVING SSTABLES AFTER COMPACTION
     * 
     * After compaction, we remove the old SSTables that were merged.
     * We match by extracting the ID from the filename.
     * 
     * Fixed: Use a single pass to remove all matching SSTables, not one loop per ID.
     * The previous implementation had a bug where it would call remove_if multiple times.
     */
    
    // Create a set for O(1) lookup
    std::unordered_set<uint64_t> ids_to_remove(ids.begin(), ids.end());
    
    // Single pass: partition SSTables into keep/remove
    auto it = std::remove_if(sstables_.begin(), sstables_.end(),
        [&ids_to_remove](const std::unique_ptr<SSTableReader>& sstable) {
            std::string filename = sstable->file_path().filename().string();
            std::string prefix = "sstable_";
            std::string suffix = ".sst";
            if (filename.size() > prefix.size() + suffix.size()) {
                std::string id_str = filename.substr(prefix.size(), 
                    filename.size() - prefix.size() - suffix.size());
                uint64_t id = std::stoull(id_str);
                return ids_to_remove.count(id) > 0;
            }
            return false;
        });
    
    // Update total size before removing
    for (auto iter = it; iter != sstables_.end(); ++iter) {
        std::error_code ec;
        auto file_size = std::filesystem::file_size((*iter)->file_path(), ec);
        if (!ec) {
            total_size_ -= file_size;
        }
    }
    
    // Remove all matching SSTables in one call
    sstables_.erase(it, sstables_.end());
}

uint64_t Level::GetTotalSize() const {
    return total_size_;
}

bool Level::NeedsCompaction() const {
    /**
     * COMPACTION TRIGGERS
     * 
     * Each level has different rules for when compaction is needed:
     * - L0: Based on file count (4+ files) because keys can overlap
     * - L1+: Based on total size (exceeds 10x previous level)
     */
    if (level_num_ == 0) {
        // L0 uses file count instead of size
        return sstables_.size() >= 4;
    } else {
        // L1+ uses size threshold
        return total_size_ >= GetMaxSize();
    }
}

uint64_t Level::GetMaxSize() const {
    /**
     * LEVEL SIZE LIMITS
     * 
     * Each level is 10x the size of the previous level.
     * This creates a "pyramid" structure:
     * 
     * L0: 4 files (special case)
     * L1: 10 MB
     * L2: 100 MB
     * L3: 1 GB
     * L4: 10 GB
     * 
     * The multiplier (10x) is the "growth factor" - databases like RocksDB
     * use 10x as a good balance between read and write amplification.
     */
    if (level_num_ == 0) {
        return 0;  // L0 doesn't use size-based limit
    }
    
    // L1: 10 MB, L2: 100 MB, L3: 1 GB, etc.
    uint64_t base_size = 10 * 1024 * 1024;  // 10 MB for L1
    uint64_t multiplier = 1;
    for (int i = 1; i < level_num_; ++i) {
        multiplier *= 10;
    }
    return base_size * multiplier;
}

// ============================================================================
// LeveledLSM Implementation
// ============================================================================

LeveledLSM::LeveledLSM() {
    /**
     * INITIALIZATION
     * 
     * Start with just L0. Other levels are created on-demand as data
     * gets compacted down.
     */
    levels_.push_back(std::make_unique<Level>(0));  // Create L0
}

void LeveledLSM::AddL0SSTable(std::unique_ptr<SSTableReader> sstable) {
    /**
     * ADDING TO L0
     * 
     * New SSTables from MemTable flushes always go to L0.
     * L0 is special - it can have overlapping key ranges because
     * each SSTable represents a snapshot of the MemTable at different times.
     */
    if (levels_.empty()) {
        levels_.push_back(std::make_unique<Level>(0));
    }
    levels_[0]->AddSSTable(std::move(sstable));
}

Level* LeveledLSM::GetLevel(int level_num) {
    if (level_num < 0 || level_num >= static_cast<int>(levels_.size())) {
        return nullptr;
    }
    return levels_[level_num].get();
}

std::vector<SSTableReader*> LeveledLSM::GetAllSSTables() const {
    /**
     * GET ALL SSTABLES FOR READS
     * 
     * Returns SSTables in search order: L0 first (newest data),
     * then L1, L2, etc. (older data).
     * 
     * This ordering is critical for correctness - we must check
     * newer levels first to get the most recent value for a key.
     */
    std::vector<SSTableReader*> result;
    for (const auto& level : levels_) {
        for (const auto& sstable : level->GetSSTables()) {
            result.push_back(sstable.get());
        }
    }
    return result;
}

LeveledLSM::CompactionResult LeveledLSM::MaybeCompact(const std::filesystem::path& db_dir, uint64_t& next_sstable_id) {
    /**
     * COMPACTION DECISION
     * 
     * Check levels from top to bottom. If any level needs compaction,
     * perform it and return CompactionResult describing what changed.
     * 
     * We check in order (L0, L1, L2, ...) because L0 compaction is
     * most urgent (it blocks writes if too full).
     */
    
    // Check L0 first
    if (!levels_.empty() && levels_[0]->NeedsCompaction()) {
        return CompactL0ToL1(db_dir, next_sstable_id);
    }
    
    // Check other levels
    for (size_t i = 1; i < levels_.size(); ++i) {
        if (levels_[i]->NeedsCompaction()) {
            return CompactLevelN(static_cast<int>(i), db_dir, next_sstable_id);
        }
    }
    
    return {};  // No compaction needed (performed=false)
}

LeveledLSM::CompactionResult LeveledLSM::CompactL0ToL1(const std::filesystem::path& db_dir, uint64_t& next_sstable_id) {
    /**
     * L0 → L1 COMPACTION
     * 
     * This is the most complex compaction because L0 SSTables can have
     * overlapping keys. We must merge ALL L0 SSTables together, and also
     * merge with any overlapping L1 SSTables.
     * 
     * Algorithm:
     * 1. Collect all entries from all L0 SSTables
     * 2. Find L1 SSTables that overlap with L0's key range
     * 3. Merge everything together (newer values win)
     * 4. Write new L1 SSTables
     * 5. Delete old L0 and overlapping L1 SSTables
     */
    
    if (levels_.empty() || levels_[0]->GetSSTableCount() == 0) {
        return {};  // Nothing to compact
    }
    
    // Ensure L1 exists
    if (levels_.size() < 2) {
        levels_.push_back(std::make_unique<Level>(1));
    }
    
    Level* l0 = levels_[0].get();
    Level* l1 = levels_[1].get();
    
    // Collect all entries from L0 and L1
    // std::map keeps entries sorted and automatically handles duplicates
    // (newer values overwrite older values)
    std::map<std::string, std::string> merged_entries;
    
    // Add L1 entries first (older data)
    for (const auto& sstable : l1->GetSSTables()) {
        auto entries = sstable->GetAllSorted();
        for (const auto& [key, value] : entries) {
            merged_entries[key] = value;
        }
    }
    
    // Add L0 entries second (newer data, will overwrite L1 values)
    for (const auto& sstable : l0->GetSSTables()) {
        auto entries = sstable->GetAllSorted();
        for (const auto& [key, value] : entries) {
            merged_entries[key] = value;  // Newer value wins
        }
    }
    
    // Track old SSTable IDs for deletion and result reporting
    std::vector<uint64_t> old_l0_ids;
    std::vector<uint64_t> old_l1_ids;
    
    for (const auto& sstable : l0->GetSSTables()) {
        std::string filename = sstable->file_path().filename().string();
        std::string prefix = "sstable_";
        std::string suffix = ".sst";
        if (filename.size() > prefix.size() + suffix.size()) {
            std::string id_str = filename.substr(prefix.size(), 
                filename.size() - prefix.size() - suffix.size());
            old_l0_ids.push_back(std::stoull(id_str));
        }
    }
    
    for (const auto& sstable : l1->GetSSTables()) {
        std::string filename = sstable->file_path().filename().string();
        std::string prefix = "sstable_";
        std::string suffix = ".sst";
        if (filename.size() > prefix.size() + suffix.size()) {
            std::string id_str = filename.substr(prefix.size(), 
                filename.size() - prefix.size() - suffix.size());
            old_l1_ids.push_back(std::stoull(id_str));
        }
    }
    
    // Write merged data to new L1 SSTable
    const uint64_t compacted_id = next_sstable_id++;
    
    // Create level-specific directory if using structured layout
    std::filesystem::path level_dir = db_dir / "level_1";
    {
        std::error_code ec_mkdir;
        std::filesystem::create_directories(level_dir, ec_mkdir);
        // Ignore errors - directory might already exist
    }
    
    const auto compacted_path = level_dir / ("sstable_" + std::to_string(compacted_id) + ".sst");
    
    SSTableWriter writer(compacted_path);
    if (!writer.Open().ok()) {
        return {};
    }
    
    for (const auto& [key, value] : merged_entries) {
        // Skip tombstones that have shadowed all older values
        // (optimization: we can remove them now)
        if (value == MemTable::kTombstoneValue) {
            // In a full implementation, we'd only remove tombstones if
            // we're sure there are no older values in lower levels.
            // For now, keep them to be safe.
        }
        
        if (!writer.Add(key, value).ok()) {
            return {};
        }
    }
    
    if (!writer.Finish().ok()) {
        return {};
    }
    
    // Open the new SSTable for reading
    auto new_sstable = std::make_unique<SSTableReader>(compacted_path);
    if (!new_sstable->Open().ok()) {
        return {};
    }
    
    // Update levels: remove old SSTables, add new one
    l0->RemoveSSTables(old_l0_ids);
    l1->RemoveSSTables(old_l1_ids);
    l1->AddSSTable(std::move(new_sstable));
    
    // Delete old SSTable files (search level directories)
    std::error_code ec;
    for (uint64_t id : old_l0_ids) {
        // Try level_0 first, then fallback to root
        std::filesystem::path path = db_dir / "level_0" / ("sstable_" + std::to_string(id) + ".sst");
        if (!std::filesystem::exists(path)) {
            path = db_dir / ("sstable_" + std::to_string(id) + ".sst");
        }
        std::filesystem::remove(path, ec);
    }
    for (uint64_t id : old_l1_ids) {
        // Try level_1 first, then fallback to root
        std::filesystem::path path = db_dir / "level_1" / ("sstable_" + std::to_string(id) + ".sst");
        if (!std::filesystem::exists(path)) {
            path = db_dir / ("sstable_" + std::to_string(id) + ".sst");
        }
        std::filesystem::remove(path, ec);
    }
    
    // Return compaction result with IDs for manifest update
    CompactionResult result;
    result.performed = true;
    result.added_ids.push_back(compacted_id);
    result.removed_ids = old_l0_ids;
    result.removed_ids.insert(result.removed_ids.end(), old_l1_ids.begin(), old_l1_ids.end());
    return result;
}

LeveledLSM::CompactionResult LeveledLSM::CompactLevelN(int level_num, const std::filesystem::path& db_dir, uint64_t& next_sstable_id) {
    /**
     * LN → L(N+1) COMPACTION
     * 
     * Compacting between non-L0 levels is simpler because keys don't overlap
     * within a level. We can pick just one SSTable from the level and merge
     * it with overlapping SSTables in the next level.
     * 
     * For simplicity, we'll merge all SSTables in the level for now
     * (a production implementation would be more selective).
     */
    
    if (level_num < 1 || level_num >= static_cast<int>(levels_.size())) {
        return {};
    }
    
    // Ensure next level exists
    if (level_num + 1 >= static_cast<int>(levels_.size())) {
        levels_.push_back(std::make_unique<Level>(level_num + 1));
    }
    
    Level* source_level = levels_[level_num].get();
    Level* target_level = levels_[level_num + 1].get();
    
    if (source_level->GetSSTableCount() == 0) {
        return {};
    }
    
    // Merge all entries (similar to L0→L1)
    std::map<std::string, std::string> merged_entries;
    
    // Add target level entries first (older)
    for (const auto& sstable : target_level->GetSSTables()) {
        auto entries = sstable->GetAllSorted();
        for (const auto& [key, value] : entries) {
            merged_entries[key] = value;
        }
    }
    
    // Add source level entries second (newer)
    for (const auto& sstable : source_level->GetSSTables()) {
        auto entries = sstable->GetAllSorted();
        for (const auto& [key, value] : entries) {
            merged_entries[key] = value;
        }
    }
    
    // Track old IDs for manifest update
    std::vector<uint64_t> old_source_ids;
    std::vector<uint64_t> old_target_ids;
    
    for (const auto& sstable : source_level->GetSSTables()) {
        std::string filename = sstable->file_path().filename().string();
        std::string prefix = "sstable_";
        std::string suffix = ".sst";
        if (filename.size() > prefix.size() + suffix.size()) {
            std::string id_str = filename.substr(prefix.size(), 
                filename.size() - prefix.size() - suffix.size());
            old_source_ids.push_back(std::stoull(id_str));
        }
    }
    
    for (const auto& sstable : target_level->GetSSTables()) {
        std::string filename = sstable->file_path().filename().string();
        std::string prefix = "sstable_";
        std::string suffix = ".sst";
        if (filename.size() > prefix.size() + suffix.size()) {
            std::string id_str = filename.substr(prefix.size(), 
                filename.size() - prefix.size() - suffix.size());
            old_target_ids.push_back(std::stoull(id_str));
        }
    }
    
    // Write new SSTable to target level
    const uint64_t compacted_id = next_sstable_id++;
    
    // Create level-specific directory for target level
    std::filesystem::path level_dir = db_dir / ("level_" + std::to_string(target_level->GetLevelNum()));
    {
        std::error_code ec_mkdir;
        std::filesystem::create_directories(level_dir, ec_mkdir);
        // Ignore errors - directory might already exist
    }
    
    const auto compacted_path = level_dir / ("sstable_" + std::to_string(compacted_id) + ".sst");
    
    SSTableWriter writer(compacted_path);
    if (!writer.Open().ok()) {
        return {};
    }
    
    for (const auto& [key, value] : merged_entries) {
        if (!writer.Add(key, value).ok()) {
            return {};
        }
    }
    
    if (!writer.Finish().ok()) {
        return {};
    }
    
    auto new_sstable = std::make_unique<SSTableReader>(compacted_path);
    if (!new_sstable->Open().ok()) {
        return {};
    }
    
    // Update levels
    source_level->RemoveSSTables(old_source_ids);
    target_level->RemoveSSTables(old_target_ids);
    target_level->AddSSTable(std::move(new_sstable));
    
    // Delete old SSTable files (search appropriate level directories)
    std::error_code ec;
    int source_level_num = source_level->GetLevelNum();
    int target_level_num = target_level->GetLevelNum();
    
    for (uint64_t id : old_source_ids) {
        // Try source level directory first
        std::filesystem::path path = db_dir / ("level_" + std::to_string(source_level_num)) / ("sstable_" + std::to_string(id) + ".sst");
        if (!std::filesystem::exists(path)) {
            path = db_dir / ("sstable_" + std::to_string(id) + ".sst");
        }
        std::filesystem::remove(path, ec);
    }
    for (uint64_t id : old_target_ids) {
        // Try target level directory first
        std::filesystem::path path = db_dir / ("level_" + std::to_string(target_level_num)) / ("sstable_" + std::to_string(id) + ".sst");
        if (!std::filesystem::exists(path)) {
            path = db_dir / ("sstable_" + std::to_string(id) + ".sst");
        }
        std::filesystem::remove(path, ec);
    }
    
    // Return compaction result
    CompactionResult result;
    result.performed = true;
    result.added_ids.push_back(compacted_id);
    result.removed_ids = old_source_ids;
    result.removed_ids.insert(result.removed_ids.end(), old_target_ids.begin(), old_target_ids.end());
    return result;
}

}  // namespace core_engine
