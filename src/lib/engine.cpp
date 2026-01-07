#include <core_engine/engine.hpp>

// core_engine/engine.cpp
//
// Purpose:
// - Implement the Engine façade for Year 1 Q1 (Page & Disk Layer).
// - Simple key-value storage using DiskManager for page I/O.
// - BufferPoolManager will be added in Year 1 Q2.

#include <core_engine/common/logger.hpp>

#include <chrono>
#include <filesystem>
#include <sstream>
#include <utility>

namespace core_engine {

Engine::Engine() = default;

Status Engine::Open(std::filesystem::path db_path) {
  return Open(DatabaseConfig::Embedded(std::move(db_path)));
}

Status Engine::Open(const DatabaseConfig& config) {
  if (is_open_) {
    return Status::AlreadyExists("Engine already open");
  }

  config_ = config;

  // Create database directory if it doesn't exist
  if (!std::filesystem::exists(config_.data_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(config_.data_dir, ec);
    if (ec) {
      return Status::IoError("Failed to create data directory: " + ec.message());
    }
  }

  // Create DiskManager for page I/O
  auto db_file = config_.data_dir / "pages.db";
  disk_manager_ = std::make_unique<DiskManager>(db_file);

  auto status = disk_manager_->Open();
  if (!status.ok()) {
    Log(LogLevel::kError, "Failed to open DiskManager: " + status.ToString());
    return status;
  }

  // Initialize vector index if enabled
  if (config_.enable_vector_index) {
    vector::HNSWIndex::Params hnsw_params;
    hnsw_params.dimension = config_.vector_dimension;
    
    // Convert config metric to vector metric
    switch (config_.vector_metric) {
      case DatabaseConfig::VectorDistanceMetric::kCosine:
        hnsw_params.metric = vector::DistanceMetric::kCosine;
        break;
      case DatabaseConfig::VectorDistanceMetric::kEuclidean:
        hnsw_params.metric = vector::DistanceMetric::kEuclidean;
        break;
      case DatabaseConfig::VectorDistanceMetric::kDotProduct:
        hnsw_params.metric = vector::DistanceMetric::kDotProduct;
        break;
      case DatabaseConfig::VectorDistanceMetric::kManhattan:
        hnsw_params.metric = vector::DistanceMetric::kManhattan;
        break;
    }
    
    hnsw_params.M = config_.hnsw_params.M;
    hnsw_params.ef_construction = config_.hnsw_params.ef_construction;
    hnsw_params.ef_search = config_.hnsw_params.ef_search;
    
    vector_index_ = std::make_unique<vector::HNSWIndex>(hnsw_params);
    Log(LogLevel::kInfo,
        "Vector index initialized (dimension=" + std::to_string(config_.vector_dimension) + ")");
  }

  is_open_ = true;
  Log(LogLevel::kInfo, "Engine opened (Year 1 Q1 - Page Layer)");
  return Status::Ok();
}

Status Engine::Put(std::string key, std::string value) {
  if (!is_open_) {
    return Status::Internal("Engine is not open");
  }

  // Year 1 Q1: Simplified implementation
  // Future: Use proper page-based KV storage with B-tree or hash table
  ++total_puts_;

  Log(LogLevel::kDebug, "Put: " + key + " = " + value);
  return Status::Ok();
}

std::optional<std::string> Engine::Get(std::string key) {
  if (!is_open_) {
    return std::nullopt;
  }

  // Year 1 Q1: Simplified implementation
  // Future: Implement proper page-based lookup
  ++total_gets_;

  Log(LogLevel::kDebug, "Get: " + key);
  return std::nullopt;
}

Status Engine::Delete(std::string key) {
  if (!is_open_) {
    return Status::Internal("Engine is not open");
  }

  // Year 1 Q1: Simplified implementation
  Log(LogLevel::kDebug, "Delete: " + key);
  return Status::Ok();
}

Status Engine::BatchWrite(const std::vector<BatchOperation>& operations) {
  for (const auto& op : operations) {
    Status status = Status::Ok(); // Initialize with Ok status
    if (op.type == BatchOperation::Type::PUT) {
      status = Put(op.key, op.value);
    } else {
      status = Delete(op.key);
    }

    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

std::vector<std::optional<std::string>> Engine::BatchGet(const std::vector<std::string>& keys) {
  std::vector<std::optional<std::string>> results;
  results.reserve(keys.size());

  for (const auto& key : keys) {
    results.push_back(Get(key));
  }

  return results;
}

std::vector<std::pair<std::string, std::string>> 
Engine::Scan(const std::string& start_key, const std::string& end_key, const ScanOptions& options) {
  // Year 1 Q1: Placeholder
  return {};
}

Status Engine::Execute(std::string_view statement) {
  return Status::Unimplemented("SQL execution not implemented (Year 1 Q1 focuses on page layer)");
}

Engine::Stats Engine::GetStats() const {
  Stats stats{};

  if (disk_manager_) {
    auto disk_stats = disk_manager_->GetStats();
    stats.total_pages = disk_manager_->GetNumPages();
    stats.total_reads = disk_stats.total_reads;
    stats.total_writes = disk_stats.total_writes;
    stats.checksum_failures = disk_stats.checksum_failures;
  }

  stats.total_gets = total_gets_;
  stats.total_puts = total_puts_;
  stats.avg_get_time_us = (total_gets_ > 0) ? (total_get_time_us_ / total_gets_) : 0.0;
  stats.avg_put_time_us = (total_puts_ > 0) ? (total_put_time_us_ / total_puts_) : 0.0;

  return stats;
}

std::vector<std::pair<std::string, std::string>> Engine::GetAllEntries() const {
  // Year 1 Q1: Placeholder
  return {};
}

// ====== Vector Database Operations ======

Status Engine::PutVector(const std::string& key, const vector::Vector& vec) {
  if (!vector_index_) {
    return Status::Internal("Vector index not enabled");
  }

  if (vec.dimension() != config_.vector_dimension) {
    return Status::InvalidArgument("Vector dimension mismatch");
  }

  // Insert into HNSW index
  auto status = vector_index_->Insert(key, vec);
  if (!status.ok()) {
    return status;
  }

  // Store serialized vector (Year 1 Q1: simplified)
  std::string serialized = vec.Serialize();
  return Put(key, serialized);
}

std::vector<Engine::VectorSearchResult> Engine::SearchSimilar(const vector::Vector& query,
                                                              std::size_t k, bool include_vectors) {
  std::vector<VectorSearchResult> results;

  if (!vector_index_) {
    return results;
  }

  auto hnsw_results = vector_index_->Search(query, k);

  for (const auto& result : hnsw_results) {
    VectorSearchResult vr;
    vr.key = result.key;
    vr.distance = result.distance;

    if (include_vectors) {
      auto vec_opt = GetVector(result.key);
      if (vec_opt.has_value()) {
        vr.vector = *vec_opt;
      }
    }

    results.push_back(std::move(vr));
  }

  return results;
}

std::optional<vector::Vector> Engine::GetVector(const std::string& key) {
  auto serialized_opt = Get(key);
  if (!serialized_opt.has_value()) {
    return std::nullopt;
  }

  return vector::Vector::Deserialize(*serialized_opt);
}

Status Engine::BatchPutVectors(const std::vector<std::pair<std::string, vector::Vector>>& vectors) {
  for (const auto& [key, vec] : vectors) {
    auto status = PutVector(key, vec);
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

std::vector<std::optional<vector::Vector>>
Engine::BatchGetVectors(const std::vector<std::string>& keys) {
  std::vector<std::optional<vector::Vector>> results;
  results.reserve(keys.size());
  
  for (const auto& key : keys) {
    results.push_back(GetVector(key));
  }
  
  return results;
}

Engine::VectorStats Engine::GetVectorStats() const {
  VectorStats stats;

  if (!vector_index_) {
    return stats;
  }

  stats.index_enabled = true;
  stats.dimension = vector_index_->dimension();
  stats.metric = vector::ToString(vector_index_->metric());

  auto hnsw_stats = vector_index_->GetStats();
  stats.num_vectors = hnsw_stats.num_vectors;
  stats.num_layers = hnsw_stats.num_layers;
  stats.avg_connections_per_node = hnsw_stats.avg_connections_per_node;

  return stats;
}

}  // namespace core_engine
