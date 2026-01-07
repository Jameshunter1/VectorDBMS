#pragma once

// This header defines the smallest “public” KV surface for an LSM-first engine.
// The goal is to keep the first milestone concrete and testable.

#include <optional>  // std::optional for Get results.
#include <string>    // std::string owns key/value bytes.

#include <core_engine/common/status.hpp>  // core_engine::Status.

namespace core_engine {

// KeyValueStore is the minimal API you can build an LSM on top of.
//
// Why keep this separate from SQL execution?
// - An LSM is fundamentally a KV store.
// - SQL layers can come later without blocking storage correctness.
class KeyValueStore {
 public:
  virtual ~KeyValueStore() = default;

  // Put inserts or overwrites a value for a key.
  virtual Status Put(std::string key, std::string value) = 0;

  // Get returns the value if present.
  virtual std::optional<std::string> Get(std::string key) = 0;

  // Delete removes a key (uses tombstone internally).
  virtual Status Delete(std::string key) = 0;
};

}  // namespace core_engine
