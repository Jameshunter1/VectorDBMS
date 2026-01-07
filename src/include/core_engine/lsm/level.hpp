#pragma once

/**
 * MULTI-LEVEL LSM-TREE
 * 
 * What is a leveled LSM-tree?
 * Instead of keeping all SSTables in a flat list, we organize them into levels.
 * Each level has different size limits and compaction rules.
 * 
 * Level Structure:
 * - Level 0 (L0): Fresh SSTables from MemTable flushes (4-8 files, overlapping keys)
 * - Level 1 (L1): First compacted level (10x size of L0, non-overlapping files)
 * - Level 2 (L2): 10x size of L1, non-overlapping files
 * - Level N (LN): 10x size of L(N-1), non-overlapping files
 * 
 * Why levels?
 * - **Faster reads**: Most data is in L1+, which has non-overlapping SSTables
 *   (no need to check every file, just find the one that might contain the key)
 * - **Less write amplification**: Smaller, incremental compactions instead of
 *   merging all SSTables at once
 * - **Space amplification control**: Old data gets pushed to lower levels
 * 
 * Example:
 * L0: [sstable_0.sst, sstable_1.sst, sstable_2.sst] ← overlapping keys
 * L1: [sstable_3.sst, sstable_4.sst]                 ← non-overlapping
 * L2: [sstable_5.sst, sstable_6.sst, sstable_7.sst] ← non-overlapping
 * 
 * Compaction triggers:
 * - L0 → L1: When L0 has 4+ files (current threshold)
 * - L1 → L2: When L1 exceeds 10 MB
 * - L2 → L3: When L2 exceeds 100 MB
 */

#include <core_engine/lsm/sstable.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core_engine {

/**
 * Level represents a single level in the LSM-tree.
 * Each level contains multiple SSTables and tracks its size.
 */
class Level {
public:
    /**
     * Creates a level with a specific level number.
     * 
     * @param level_num The level number (0, 1, 2, ...)
     *                  L0 is special (allows overlapping keys)
     *                  L1+ have non-overlapping keys
     */
    explicit Level(int level_num);

    /**
     * Adds an SSTable to this level.
     * 
     * @param sstable The SSTable to add (takes ownership via unique_ptr)
     */
    void AddSSTable(std::unique_ptr<SSTableReader> sstable);

    /**
     * Removes SSTables by their IDs (used after compaction).
     * 
     * @param ids Vector of SSTable IDs to remove
     */
    void RemoveSSTables(const std::vector<uint64_t>& ids);

    /**
     * Gets all SSTables in this level.
     */
    const std::vector<std::unique_ptr<SSTableReader>>& GetSSTables() const {
        return sstables_;
    }

    /**
     * Returns the number of SSTables in this level.
     */
    size_t GetSSTableCount() const {
        return sstables_.size();
    }

    /**
     * Returns the total size of all SSTables in this level (bytes).
     * This is used to determine when compaction should trigger.
     */
    uint64_t GetTotalSize() const;

    /**
     * Returns the level number (0, 1, 2, ...).
     */
    int GetLevelNum() const {
        return level_num_;
    }

    /**
     * Checks if this level needs compaction.
     * Different levels have different thresholds.
     */
    bool NeedsCompaction() const;

    /**
     * Gets the maximum size for this level in bytes.
     * L0: 4 files (not size-based)
     * L1: 10 MB
     * L2: 100 MB
     * L3: 1 GB
     * Each level is 10x the previous
     */
    uint64_t GetMaxSize() const;

private:
    int level_num_;                                      // Level number (0, 1, 2, ...)
    std::vector<std::unique_ptr<SSTableReader>> sstables_; // SSTables in this level
    uint64_t total_size_ = 0;                            // Total size in bytes
};

/**
 * LeveledLSM manages multiple levels of SSTables.
 * This is the controller that decides when and how to compact.
 */
class LeveledLSM {
public:
    LeveledLSM();

    /**
     * Adds a new SSTable from a MemTable flush.
     * Always goes to L0 first.
     */
    void AddL0SSTable(std::unique_ptr<SSTableReader> sstable);

    /**
     * Result of a compaction operation.
     * Contains the IDs of SSTables that were added and removed.
     */
    struct CompactionResult {
        bool performed = false;                           // True if compaction happened
        std::vector<uint64_t> added_ids;                  // New SSTable IDs created
        std::vector<uint64_t> removed_ids;                // Old SSTable IDs deleted
    };

    /**
     * Performs compaction if any level exceeds its threshold.
     * Returns CompactionResult describing what changed.
     * 
     * The caller (LsmTree) must update the Manifest with the returned IDs.
     */
    CompactionResult MaybeCompact(const std::filesystem::path& db_dir, uint64_t& next_sstable_id);

    /**
     * Gets all SSTables across all levels (for reads).
     * Returns them in order: L0 first (newest), then L1, L2, etc.
     */
    std::vector<SSTableReader*> GetAllSSTables() const;

    /**
     * Gets the number of levels currently in use.
     */
    size_t GetLevelCount() const {
        return levels_.size();
    }

    /**
     * Gets a specific level (returns nullptr if doesn't exist).
     */
    Level* GetLevel(int level_num);

private:
    /**
     * Compacts L0 into L1.
     * Merges all L0 SSTables with overlapping L1 SSTables.
     */
    CompactionResult CompactL0ToL1(const std::filesystem::path& db_dir, uint64_t& next_sstable_id);

    /**
     * Compacts a level into the next level.
     * More efficient than L0→L1 because keys don't overlap.
     */
    CompactionResult CompactLevelN(int level_num, const std::filesystem::path& db_dir, uint64_t& next_sstable_id);

    std::vector<std::unique_ptr<Level>> levels_;         // Levels 0, 1, 2, ...
};

}  // namespace core_engine
