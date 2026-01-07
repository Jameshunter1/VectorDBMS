#pragma once

// core_engine/execution/executor.hpp
//
// Purpose:
// - Explicit boundary for a future query execution layer.
// - For a page-based roadmap, this stays stubbed until page layer and
//   durability (WAL + recovery + SSTables) are solid.

#include <string_view>

#include <core_engine/common/status.hpp>

namespace core_engine {

// Executor is a placeholder for a future execution engine.
//
// This is where a query plan would be executed against storage.
// Keeping the boundary explicit early makes long-term refactors less painful.
class Executor {
 public:
  Status Execute(std::string_view statement);
};

}  // namespace core_engine
