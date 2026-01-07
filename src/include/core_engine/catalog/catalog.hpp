#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <core_engine/common/status.hpp>

namespace core_engine {

// Catalog holds schema metadata.
//
// In mature engines this is a persistent, transactional subsystem.
// For the starter template we keep it in-memory to demonstrate the boundary.
class Catalog {
 public:
  Status CreateTable(std::string name);
  bool HasTable(std::string_view name) const;

 private:
  std::unordered_map<std::string, bool> tables_;
};

}  // namespace core_engine
