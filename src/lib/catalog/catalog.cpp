#include <core_engine/catalog/catalog.hpp>

namespace core_engine {

Status Catalog::CreateTable(std::string name) {
  if (name.empty()) {
    return Status::InvalidArgument("Table name cannot be empty");
  }

  if (tables_.contains(name)) {
    return Status::AlreadyExists("Table already exists");
  }

  tables_.emplace(std::move(name), true);
  return Status::Ok();
}

bool Catalog::HasTable(std::string_view name) const {
  return tables_.contains(std::string(name));
}

} // namespace core_engine
