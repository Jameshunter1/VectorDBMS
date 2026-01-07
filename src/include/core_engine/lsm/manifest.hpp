#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace core_engine {

/**
 * MANIFEST FILE
 * 
 * What it does:
 * The manifest is a log that tracks which SSTable files are currently active
 * in the database. It's essential for recovering the database state after a restart.
 * 
 * Why we need it:
 * Without a manifest, when you restart the database, you don't know:
 * - Which SSTables exist and should be loaded
 * - What the next SSTable ID should be
 * - Which SSTables were deleted during compaction
 * 
 * How it works:
 * - Each line in the manifest represents an operation (ADD or REMOVE)
 * - Format: "ADD 5\n" or "REMOVE 3\n"
 * - On startup, we replay all operations to rebuild the list of active SSTables
 * 
 * Example manifest file:
 *   ADD 0
 *   ADD 1
 *   ADD 2
 *   REMOVE 0    <- SSTable 0 was deleted during compaction
 *   REMOVE 1    <- SSTable 1 was deleted during compaction
 *   ADD 3       <- New compacted SSTable created
 * 
 * After replay, active SSTables are: [2, 3]
 */

class Manifest {
public:
    /**
     * Opens or creates a manifest file at the specified path.
     * If the file exists, it reads and validates it but doesn't replay it yet.
     * 
     * @param path Path to the manifest file (e.g., "./mydb/MANIFEST")
     * @return true if successful, false if the file is corrupted
     */
    bool Open(const std::filesystem::path& path);

    /**
     * Reads the manifest and returns the list of active SSTable IDs.
     * This is called on database startup to know which SSTables to load.
     * 
     * How it works:
     * - Reads each line from the manifest file
     * - "ADD N" adds N to the active set
     * - "REMOVE N" removes N from the active set
     * - Returns the final set of IDs that should be loaded
     * 
     * @param out_sstable_ids Vector to fill with active SSTable IDs
     * @return true if successful, false if the manifest is corrupted
     */
    bool GetActiveSSTables(std::vector<uint64_t>& out_sstable_ids);

    /**
     * Records that a new SSTable has been created.
     * Appends "ADD <id>\n" to the manifest file.
     * 
     * Example: When MemTable flushes to disk as sstable_5.sst, call AddSSTable(5)
     * 
     * @param id The SSTable ID that was just created
     */
    void AddSSTable(uint64_t id);

    /**
     * Records that SSTables have been removed (usually during compaction).
     * Appends "REMOVE <id>\n" for each ID to the manifest file.
     * 
     * Example: After compacting sstable_0.sst and sstable_1.sst into sstable_5.sst,
     *          call RemoveSSTables({0, 1}) to mark them as deleted.
     * 
     * @param ids Vector of SSTable IDs that were deleted
     */
    void RemoveSSTables(const std::vector<uint64_t>& ids);

    /**
     * Closes the manifest file.
     * All pending writes are flushed to disk to ensure durability.
     */
    void Close();

    /**
     * Checks if the manifest is currently open and ready for use.
     */
    bool IsOpen() const { return is_open_; }

private:
    std::filesystem::path path_;  // Path to the manifest file (e.g., "./mydb/MANIFEST")
    bool is_open_ = false;        // Whether the file is currently open
};

}  // namespace core_engine
