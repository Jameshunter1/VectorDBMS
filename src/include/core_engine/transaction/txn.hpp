#pragma once

#include <cstdint>

namespace core_engine {

// Transaction is intentionally small. A transaction is a tag for a series of operations that should be atomic and isolated.

// Future directions:
// - MVCC timestamps
// - lock manager integration
// - isolation levels
// - undo buffer
using TxnId = std::uint64_t;

class Txn {
 public:
  explicit Txn(TxnId id) : id_(id) {}

  TxnId id() const { return id_; }

 private:
  TxnId id_;
};

}  // namespace core_engine
