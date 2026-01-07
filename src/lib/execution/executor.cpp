#include <core_engine/execution/executor.hpp>

namespace core_engine {

Status Executor::Execute(std::string_view /*statement*/) {
  // Not implemented yet. This is a deliberate milestone boundary.
  return Status::Unimplemented("Execution engine not implemented");
}

} // namespace core_engine
