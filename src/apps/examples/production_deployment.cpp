// Production Deployment Example
// Demonstrates using DatabaseConfig for server deployment

#include <core_engine/engine.hpp>
#include <core_engine/common/config.hpp>
#include <core_engine/common/logger.hpp>
#include <iostream>

using namespace core_engine;

int main() {
    // Example 1: Embedded Mode (SQLite-style)
    // All files in one directory - perfect for desktop apps
    {
        std::cout << "=== Example 1: Embedded Mode ===\n";
        
        Engine engine;
        auto status = engine.Open("./my_app_data");
        if (!status.ok()) {
            std::cerr << "Failed to open embedded database: " << status.ToString() << "\n";
            return 1;
        }
        
        // Use the database
        engine.Put("user:1", "Alice");
        auto value = engine.Get("user:1");
        std::cout << "Retrieved: " << (value ? *value : "NOT FOUND") << "\n";
        
        std::cout << "Embedded database structure:\n";
        std::cout << "  my_app_data/\n";
        std::cout << "    wal.log         (write-ahead log)\n";
        std::cout << "    MANIFEST        (SSTable registry)\n";
        std::cout <<        "    level_0/        (L0 SSTables)\n";
        std::cout << "    level_1/        (L1 SSTables)\n\n";
    }
    
    // Example 2: Production Mode with Separate Disks
    // WAL on fast SSD, data on capacity HDD
    {
        std::cout << "=== Example 2: Production Mode ===\n";
        
        // On Windows:
        // - Fast disk: C:\\ (SSD for WAL)
        // - Data disk: D:\\ (HDD for SSTables)
        //
        // On Linux:
        // - Fast disk: /mnt/nvme (NVMe SSD for WAL)
        // - Data disk: /mnt/hdd (HDD for SSTables)
        
        #ifdef _WIN32
        auto config = DatabaseConfig::Production("C:\\ProgramData\\LSMDatabase");
        // Override to put WAL on different disk
        config.wal_dir = "C:\\ProgramData\\LSMDatabase\\wal";  // Fast SSD
        config.data_dir = "D:\\LSMDatabase\\data";              // Capacity HDD
        #else
        auto config = DatabaseConfig::Production("/var/lib/lsmdb");
        config.wal_dir = "/mnt/nvme/lsmdb/wal";   // Fast NVMe
        config.data_dir = "/mnt/hdd/lsmdb/data";  // Capacity HDD
        #endif
        
        // Tune for production workload
        config.memtable_flush_threshold_bytes = 64 * 1024 * 1024;  // 64 MB (larger batches)
        config.block_cache_size_bytes = 512 * 1024 * 1024;         // 512 MB cache
        config.wal_sync_mode = DatabaseConfig::WalSyncMode::kEveryWrite;
        
        Engine engine;
        auto status = engine.Open(config);
        if (!status.ok()) {
            std::cerr << "Failed to open production database: " << status.ToString() << "\n";
            // In production, initialize directories first or use proper permissions
            std::cout << "(This is expected if directories don't exist yet)\n\n";
        } else {
            std::cout << "Production database structure:\n";
            std::cout << "  Fast Disk (C:\\ or /mnt/nvme):\n";
            std::cout << "    wal/\n";
            std::cout << "      wal.log       (sequential writes, needs fsync)\n";
            std::cout << "  Capacity Disk (D:\\ or /mnt/hdd):\n";
            std::cout << "    data/\n";
            std::cout << "      MANIFEST      (SSTable registry)\n";
            std::cout << "      level_0/      (fresh SSTables)\n";
            std::cout << "      level_1/      (compacted, non-overlapping)\n";
            std::cout << "      level_2/      (10x larger than L1)\n\n";
        }
    }
    
    // Example 3: Development Mode (Fast, Less Durable)
    {
        std::cout << "=== Example 3: Development Mode ===\n";
        
        auto config = DatabaseConfig::Development("./dev_db");
        config.wal_sync_mode = DatabaseConfig::WalSyncMode::kNone;  // Skip fsync for speed
        
        Engine engine;
        auto status = engine.Open(config);
        if (!status.ok()) {
            std::cerr << "Failed to open dev database: " << status.ToString() << "\n";
            return 1;
        }
        
        engine.Put("test", "value");
        std::cout << "Development mode: Fast writes (no fsync), data loss possible on crash\n";
        std::cout << "Perfect for testing and local development\n\n";
    }
    
    std::cout << "=== Configuration Recommendations ===\n\n";
    
    std::cout << "Desktop Application:\n";
    std::cout << "  - Use Embedded mode\n";
    std::cout << "  - Single directory in user's app data folder\n";
    std::cout << "  - Example: ~/AppData/Local/MyApp/database\n\n";
    
    std::cout << "Server Deployment:\n";
    std::cout << "  - Use Production mode\n";
    std::cout << "  - Separate WAL on fast disk (NVMe/SSD)\n";
    std::cout << "  - Data files on capacity disk (HDD acceptable)\n";
    std::cout << "  - Linux: /var/lib/lsmdb/{wal,data}\n";
    std::cout << "  - Windows: C:\\ProgramData\\LSMDatabase\\{wal,data}\n\n";
    
    std::cout << "Development:\n";
    std::cout << "  - Use Development mode\n";
    std::cout << "  - Disable fsync for speed\n";
    std::cout << "  - Local directory: ./dev_db or ./_test_db\n\n";
    
    return 0;
}
