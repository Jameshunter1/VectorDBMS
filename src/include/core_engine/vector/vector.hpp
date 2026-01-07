#pragma once

// core_engine/vector/vector.hpp
//
// Purpose:
// - Vector data type for embeddings
// - Distance/similarity metrics (cosine, euclidean, dot product)
// - Vector utilities (normalization, serialization)

#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include <core_engine/common/status.hpp>

namespace core_engine {
namespace vector {

// Vector represents a dense vector for similarity search.
// Typically used to store embeddings from ML models (e.g., text, image embeddings).
class Vector {
 public:
  using value_type = float;
  using container_type = std::vector<value_type>;
  
  // Constructors
  Vector() = default;
  explicit Vector(std::size_t dimension) : data_(dimension, 0.0f) {}
  explicit Vector(container_type data) : data_(std::move(data)) {}
  Vector(std::initializer_list<value_type> init) : data_(init) {}
  
  // Access
  value_type& operator[](std::size_t index) { return data_[index]; }
  const value_type& operator[](std::size_t index) const { return data_[index]; }
  
  std::size_t dimension() const { return data_.size(); }
  bool empty() const { return data_.empty(); }
  
  const container_type& data() const { return data_; }
  container_type& data() { return data_; }
  
  // Iterators
  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }
  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }
  
  // Serialization (binary format: dimension as uint32_t + float array)
  std::string Serialize() const;
  static Vector Deserialize(const std::string& serialized);
  
  // Vector operations
  void Normalize();  // Convert to unit vector (L2 norm = 1)
  value_type Magnitude() const;  // L2 norm
  
 private:
  container_type data_;
};

// ====== Distance Metrics ======

enum class DistanceMetric {
  kCosine,       // Cosine similarity (converted to distance: 1 - similarity)
  kEuclidean,    // L2 distance (sqrt of sum of squared differences)
  kDotProduct,   // Negative dot product (higher dot product = closer)
  kManhattan     // L1 distance (sum of absolute differences)
};

// Compute distance between two vectors based on the specified metric.
// Lower distance = more similar vectors.
// Throws std::invalid_argument if dimensions don't match.
float ComputeDistance(const Vector& a, const Vector& b, DistanceMetric metric);

// Individual distance functions (exposed for direct use)
float CosineDistance(const Vector& a, const Vector& b);      // 1 - cosine_similarity
float EuclideanDistance(const Vector& a, const Vector& b);   // L2 distance
float DotProductDistance(const Vector& a, const Vector& b);  // -dot_product (for max inner product search)
float ManhattanDistance(const Vector& a, const Vector& b);   // L1 distance

// Cosine similarity (1.0 = identical direction, 0.0 = orthogonal, -1.0 = opposite)
float CosineSimilarity(const Vector& a, const Vector& b);

// Dot product (unnormalized inner product)
float DotProduct(const Vector& a, const Vector& b);

// Utility: Convert distance metric name to enum
DistanceMetric ParseDistanceMetric(const std::string& name);
std::string ToString(DistanceMetric metric);

}  // namespace vector
}  // namespace core_engine
