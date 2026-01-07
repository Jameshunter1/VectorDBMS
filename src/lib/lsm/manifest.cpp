#include "core_engine/lsm/manifest.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace core_engine {

bool Manifest::Open(const std::filesystem::path& path) {
    path_ = path;
    
    // Create parent directory if it doesn't exist
    // This ensures the database directory is created before we try to write the manifest
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    
    // If manifest doesn't exist, create an empty one
    // This is the first-time setup case
    if (!std::filesystem::exists(path)) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return false;  // Failed to create file
        }
        file.close();
    }
    
    is_open_ = true;
    return true;
}

bool Manifest::GetActiveSSTables(std::vector<uint64_t>& out_sstable_ids) {
    out_sstable_ids.clear();
    
    if (!is_open_) {
        return false;
    }
    
    // Open the manifest file for reading
    std::ifstream file(path_);
    if (!file) {
        return false;
    }
    
    /**
     * REPLAY ALGORITHM
     * 
     * We use an unordered_set (hash set) to track active SSTables.
     * This makes ADD and REMOVE operations O(1) constant time.
     * 
     * std::unordered_set is like a hash table that only stores keys (no values).
     * It's perfect for "does this ID exist?" queries.
     */
    std::unordered_set<uint64_t> active_set;
    
    std::string line;
    uint64_t line_number = 0;
    
    // Read the manifest line by line
    // Each line is either "ADD <id>" or "REMOVE <id>"
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines (makes the file more readable for debugging)
        if (line.empty()) {
            continue;
        }
        
        /**
         * STRING PARSING
         * 
         * std::istringstream treats a string like an input stream.
         * We can use >> operator to extract words from it.
         * 
         * Example: "ADD 5" becomes:
         *   command = "ADD"
         *   id = 5
         */
        std::istringstream iss(line);
        std::string command;
        uint64_t id;
        
        // Extract command (ADD or REMOVE) and ID
        iss >> command >> id;
        
        // Check if parsing failed
        // iss.fail() returns true if >> couldn't read the data
        if (iss.fail()) {
            std::cerr << "Manifest: Invalid line " << line_number << ": " << line << "\n";
            return false;  // Corrupted manifest
        }
        
        if (command == "ADD") {
            // Add this SSTable ID to the active set
            // insert() returns a pair: (iterator, bool)
            // The bool tells us if the element was newly inserted
            active_set.insert(id);
            
        } else if (command == "REMOVE") {
            // Remove this SSTable ID from the active set
            // erase() removes the element if it exists (safe even if not found)
            active_set.erase(id);
            
        } else {
            std::cerr << "Manifest: Unknown command '" << command << "' at line " << line_number << "\n";
            return false;  // Corrupted manifest
        }
    }
    
    // Convert the set to a vector
    // The caller expects a vector, not a set
    out_sstable_ids.assign(active_set.begin(), active_set.end());
    
    // Sort the IDs for consistent ordering
    // Older SSTables (lower IDs) should be checked first
    std::sort(out_sstable_ids.begin(), out_sstable_ids.end());
    
    return true;
}

void Manifest::AddSSTable(uint64_t id) {
    if (!is_open_) {
        return;
    }
    
    /**
     * APPEND-ONLY LOG
     * 
     * std::ios::app means "append mode" — always write to the end of the file.
     * This is crucial for durability: we never overwrite old data, only add new entries.
     * 
     * Benefits:
     * - Crash-safe: If we crash mid-write, old data is intact
     * - Sequential writes: Fast on spinning disks
     * - Simple recovery: Just replay from start to end
     */
    std::ofstream file(path_, std::ios::app);  // app = append mode
    if (!file) {
        std::cerr << "Manifest: Failed to open for writing\n";
        return;
    }
    
    // Write "ADD <id>\n" to the file
    file << "ADD " << id << "\n";
    
    /**
     * FLUSH TO DISK
     * 
     * Writing to a file usually goes to an OS buffer first (in RAM).
     * We must flush() to ensure it actually hits the disk.
     * 
     * Without flush(), a crash could lose this manifest entry, causing
     * the database to "forget" about the SSTable on next restart.
     */
    file.flush();
    
    // Destructor automatically closes the file when we leave this function
}

void Manifest::RemoveSSTables(const std::vector<uint64_t>& ids) {
    if (!is_open_) {
        return;
    }
    
    std::ofstream file(path_, std::ios::app);
    if (!file) {
        std::cerr << "Manifest: Failed to open for writing\n";
        return;
    }
    
    // Write a REMOVE entry for each SSTable ID
    // Example: Compaction deletes sstable_0.sst and sstable_1.sst
    //          We write: "REMOVE 0\nREMOVE 1\n"
    for (uint64_t id : ids) {
        file << "REMOVE " << id << "\n";
    }
    
    file.flush();  // Ensure durability
}

void Manifest::Close() {
    // Nothing to do here since we open/close the file on each operation
    // This design choice makes each operation durable immediately
    is_open_ = false;
}

}  // namespace core_engine
