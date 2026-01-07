#include <core_engine/common/logger.hpp>
#include <core_engine/engine.hpp>

// core_engine/apps/dbcli/main.cpp
//
// Purpose:
// - Developer-facing CLI for basic key/value operations against an LSM-first engine.
// - Provides a fast loop to validate storage behavior without a server.
//
// Current behavior:
// - `put` appends to <db_dir>/wal.log and updates MemTable.
// - `get` reads from MemTable (until WAL recovery + SSTables are implemented).

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  using core_engine::Engine;
  using core_engine::Log;
  using core_engine::LogLevel;

  if (argc < 3) {
    std::cerr << "Usage:\n";
    std::cerr << "  dbcli <db_directory> put <key> <value>\n";
    std::cerr << "  dbcli <db_directory> get <key>\n";
    std::cerr << "  dbcli <db_directory> delete <key>\n";
    return 2;
  }

  Engine engine;
  auto status = engine.Open(argv[1]);
  if (!status.ok()) {
    Log(LogLevel::kError, status.ToString());
    return 1;
  }

  const std::string command = argv[2];

  if (command == "put") {
    if (argc < 5) {
      std::cerr << "Usage: dbcli <db_directory> put <key> <value>\n";
      return 2;
    }

    status = engine.Put(argv[3], argv[4]);
    if (!status.ok()) {
      Log(LogLevel::kError, status.ToString());
      return 1;
    }

    Log(LogLevel::kInfo, "PUT ok (written to wal.log + memtable)");
    Log(LogLevel::kInfo, "Tip: check the file <db_directory>/wal.log size");
  } else if (command == "get") {
    if (argc < 4) {
      std::cerr << "Usage: dbcli <db_directory> get <key>\n";
      return 2;
    }

    const auto value = engine.Get(argv[3]);
    if (!value.has_value()) {
      Log(LogLevel::kWarn, "Key not found");
      return 0;
    }

    std::cout << *value << "\n";
  } else if (command == "delete") {
    if (argc < 4) {
      std::cerr << "Usage: dbcli <db_directory> delete <key>\n";
      return 2;
    }

    status = engine.Delete(argv[3]);
    if (!status.ok()) {
      Log(LogLevel::kError, status.ToString());
      return 1;
    }

    Log(LogLevel::kInfo, "DELETE ok (tombstone written)");
  } else {
    std::cerr << "Unknown command: " << command << "\n";
    return 2;
  }

  Log(LogLevel::kInfo, "Done");
  return 0;
}
