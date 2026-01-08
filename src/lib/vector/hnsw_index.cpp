#include <algorithm>
#include <cmath>
#include <core_engine/vector/hnsw_index.hpp>
#include <limits>
#include <mutex>
#include <queue>

namespace core_engine {
namespace vector {

// ====== Constructor ======

HNSWIndex::HNSWIndex(Params params) : params_(std::move(params)), rng_(std::random_device{}()) {

  if (params_.dimension == 0) {
    throw std::invalid_argument("HNSW dimension must be > 0");
  }

  if (params_.M == 0) {
    throw std::invalid_argument("HNSW M (max connections) must be > 0");
  }

  // Reserve space to reduce reallocations
  nodes_.reserve(1000);
}

// ====== Insertion ======

Status HNSWIndex::Insert(const std::string& key, const Vector& vec) {
  if (vec.dimension() != params_.dimension) {
    return Status::InvalidArgument("Vector dimension mismatch");
  }

  std::unique_lock lock(mutex_);

  // Check for duplicate key
  if (key_to_node_.count(key) > 0) {
    return Status::AlreadyExists("Key already exists in HNSW index");
  }

  InsertNode(key, vec);
  return Status::Ok();
}

int HNSWIndex::InsertNode(const std::string& key, const Vector& vec) {
  // Create new node
  int node_id = static_cast<int>(nodes_.size());
  Node node;
  node.key = key;
  node.vector = vec;
  node.layer = GetRandomLevel();
  node.neighbors.resize(node.layer + 1);

  nodes_.push_back(std::move(node));
  key_to_node_[key] = node_id;

  // If this is the first node, make it the entry point
  if (entry_point_ == -1) {
    entry_point_ = node_id;
    max_layer_ = node.layer;
    return node_id;
  }

  // Search for nearest neighbors at each layer
  int current_nearest = entry_point_;

  // Navigate from top layer down to node's layer
  for (int layer = max_layer_; layer > nodes_[node_id].layer; --layer) {
    auto neighbors = SearchLayer(vec, current_nearest, 1, layer);
    if (!neighbors.empty()) {
      current_nearest = neighbors[0];
    }
  }

  // Insert at each layer from node's layer down to layer 0
  for (int layer = nodes_[node_id].layer; layer >= 0; --layer) {
    auto candidates = SearchLayer(vec, current_nearest, params_.ef_construction, layer);

    // Select M neighbors
    SelectNeighbors(node_id, candidates, params_.M, layer);

    // Add bidirectional connections
    for (int neighbor_id : nodes_[node_id].neighbors[layer]) {
      if (layer >= static_cast<int>(nodes_[neighbor_id].neighbors.size())) {
        continue; // Neighbor is not present on this layer; skip unsafe connection
      }

      nodes_[neighbor_id].neighbors[layer].insert(node_id);

      // Prune neighbor's connections if exceeded max
      if (nodes_[neighbor_id].neighbors[layer].size() > params_.M) {
        PruneConnections(neighbor_id, layer);
      }
    }

    // Update current_nearest for next layer
    if (!candidates.empty()) {
      current_nearest = candidates[0];
    }
  }

  // Update entry point if necessary
  if (nodes_[node_id].layer > max_layer_) {
    entry_point_ = node_id;
    max_layer_ = nodes_[node_id].layer;
  }

  return node_id;
}

// ====== Search ======

std::vector<HNSWIndex::SearchResult> HNSWIndex::Search(const Vector& query, std::size_t k) const {
  if (query.dimension() != params_.dimension) {
    return {}; // Dimension mismatch
  }

  std::shared_lock lock(mutex_);

  if (entry_point_ == -1 || nodes_.empty()) {
    return {}; // Empty index
  }

  // Navigate from top layer down to layer 0
  int current_nearest = entry_point_;
  for (int layer = max_layer_; layer > 0; --layer) {
    auto neighbors = SearchLayer(query, current_nearest, 1, layer);
    if (!neighbors.empty()) {
      current_nearest = neighbors[0];
    }
  }

  // Search at layer 0 with ef_search
  auto candidates = SearchLayer(query, current_nearest, std::max(params_.ef_search, k), 0);

  // Convert to results and limit to k
  std::vector<SearchResult> results;
  results.reserve(std::min(k, candidates.size()));

  for (std::size_t i = 0; i < std::min(k, candidates.size()); ++i) {
    int node_id = candidates[i];
    if (!nodes_[node_id].deleted) {
      results.push_back({nodes_[node_id].key, Distance(query, nodes_[node_id].vector)});
    }
  }

  // Sort by distance (ascending)
  std::sort(results.begin(), results.end());

  return results;
}

std::vector<int> HNSWIndex::SearchLayer(const Vector& query, int entry_point, std::size_t ef,
                                        int layer) const {
  // Priority queue for candidates (max-heap by distance)
  auto cmp = [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
    return a.first < b.first; // Max-heap
  };
  std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, decltype(cmp)>
      candidates(cmp);

  // Min-heap for results
  auto cmp_results = [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
    return a.first > b.first; // Min-heap
  };
  std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>,
                      decltype(cmp_results)>
      results(cmp_results);

  std::unordered_set<int> visited;

  float dist = Distance(query, nodes_[entry_point].vector);
  candidates.push({dist, entry_point});
  results.push({dist, entry_point});
  visited.insert(entry_point);

  while (!candidates.empty()) {
    auto [current_dist, current_id] = candidates.top();
    candidates.pop();

    // If current is farther than worst result, we're done
    if (current_dist > results.top().first) {
      break;
    }

    // Explore neighbors
    if (layer < static_cast<int>(nodes_[current_id].neighbors.size())) {
      for (int neighbor_id : nodes_[current_id].neighbors[layer]) {
        if (visited.count(neighbor_id) == 0) {
          visited.insert(neighbor_id);

          float neighbor_dist = Distance(query, nodes_[neighbor_id].vector);

          if (neighbor_dist < results.top().first || results.size() < ef) {
            candidates.push({neighbor_dist, neighbor_id});
            results.push({neighbor_dist, neighbor_id});

            // Keep only ef best results
            if (results.size() > ef) {
              results.pop();
            }
          }
        }
      }
    }
  }

  // Extract results in order (closest first)
  std::vector<int> result_ids;
  result_ids.reserve(results.size());
  while (!results.empty()) {
    result_ids.push_back(results.top().second);
    results.pop();
  }
  std::reverse(result_ids.begin(), result_ids.end());

  return result_ids;
}

// ====== Neighbor Selection (Heuristic) ======

void HNSWIndex::SelectNeighbors(int node_id, const std::vector<int>& candidates, std::size_t M,
                                int layer) {
  if (candidates.empty())
    return;

  // Simple strategy: select M closest neighbors
  std::vector<std::pair<float, int>> distances;
  distances.reserve(candidates.size());

  const Vector& node_vec = nodes_[node_id].vector;
  for (int candidate_id : candidates) {
    if (candidate_id != node_id && !nodes_[candidate_id].deleted) {
      if (layer >= static_cast<int>(nodes_[candidate_id].neighbors.size())) {
        continue; // Candidate is not resident on this layer
      }

      float dist = Distance(node_vec, nodes_[candidate_id].vector);
      distances.push_back({dist, candidate_id});
    }
  }

  // Sort by distance and take M closest
  std::sort(distances.begin(), distances.end());

  nodes_[node_id].neighbors[layer].clear();
  for (std::size_t i = 0; i < std::min(M, distances.size()); ++i) {
    nodes_[node_id].neighbors[layer].insert(distances[i].second);
  }
}

void HNSWIndex::PruneConnections(int node_id, int layer) {
  auto& neighbors = nodes_[node_id].neighbors[layer];
  if (neighbors.size() <= params_.M)
    return;

  // Compute distances to all neighbors
  std::vector<std::pair<float, int>> distances;
  distances.reserve(neighbors.size());

  const Vector& node_vec = nodes_[node_id].vector;
  for (int neighbor_id : neighbors) {
    float dist = Distance(node_vec, nodes_[neighbor_id].vector);
    distances.push_back({dist, neighbor_id});
  }

  // Keep M closest
  std::sort(distances.begin(), distances.end());
  neighbors.clear();
  for (std::size_t i = 0; i < params_.M; ++i) {
    neighbors.insert(distances[i].second);
  }
}

// ====== Layer Selection ======

int HNSWIndex::GetRandomLevel() const {
  // Exponential decay: P(layer) = exp(-layer / level_multiplier)
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  double r = dist(rng_);
  int level = static_cast<int>(-std::log(r) * params_.level_multiplier);
  return std::min(level, static_cast<int>(params_.max_layers) - 1);
}

// ====== Distance Computation ======

float HNSWIndex::Distance(const Vector& a, const Vector& b) const {
  return ComputeDistance(a, b, params_.metric);
}

// ====== Deletion ======

Status HNSWIndex::Remove(const std::string& key) {
  std::unique_lock lock(mutex_);

  auto it = key_to_node_.find(key);
  if (it == key_to_node_.end()) {
    return Status::NotFound("Key not found in HNSW index");
  }

  // Mark as deleted (lazy deletion)
  nodes_[it->second].deleted = true;
  key_to_node_.erase(it);

  return Status::Ok();
}

// ====== Statistics ======

HNSWIndex::Stats HNSWIndex::GetStats() const {
  std::shared_lock lock(mutex_);

  Stats stats;
  stats.num_vectors = nodes_.size();
  stats.num_layers = max_layer_ + 1;

  std::size_t total_connections = 0;
  for (const auto& node : nodes_) {
    for (const auto& layer_neighbors : node.neighbors) {
      total_connections += layer_neighbors.size();
    }
  }

  stats.total_connections = total_connections;
  if (!nodes_.empty()) {
    stats.avg_connections_per_node = static_cast<double>(total_connections) / nodes_.size();
  }

  return stats;
}

std::vector<std::pair<std::string, Vector>> HNSWIndex::GetAllVectors() const {
  std::shared_lock lock(mutex_);

  std::vector<std::pair<std::string, Vector>> vectors;
  vectors.reserve(nodes_.size());

  for (const auto& node : nodes_) {
    if (!node.deleted) {
      vectors.emplace_back(node.key, node.vector);
    }
  }

  return vectors;
}

// ====== Serialization (Placeholder) ======

std::string HNSWIndex::Serialize() const {
  // TODO: Implement full serialization for persistence
  // For now, return empty string
  return "";
}

HNSWIndex HNSWIndex::Deserialize(const std::string& data) {
  (void)data; // Unused parameter - TODO: Implement deserialization
  // For now, return empty index
  Params params;
  params.dimension = 128; // Default
  return HNSWIndex(params);
}

} // namespace vector
} // namespace core_engine
