#include <core_engine/vector/vector.hpp>
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <sstream>

namespace core_engine {
namespace vector {

// ====== Vector Serialization ======

std::string Vector::Serialize() const {
  // Format: [dimension:uint32_t][data:float[dimension]]
  std::string result;
  std::uint32_t dim = static_cast<std::uint32_t>(data_.size());
  
  // Reserve space: 4 bytes for dimension + dimension * 4 bytes for floats
  result.resize(sizeof(std::uint32_t) + dim * sizeof(float));
  
  // Write dimension
  std::memcpy(result.data(), &dim, sizeof(std::uint32_t));
  
  // Write float data
  std::memcpy(result.data() + sizeof(std::uint32_t), data_.data(), dim * sizeof(float));
  
  return result;
}

Vector Vector::Deserialize(const std::string& serialized) {
  if (serialized.size() < sizeof(std::uint32_t)) {
    throw std::invalid_argument("Invalid serialized vector: too short");
  }
  
  // Read dimension
  std::uint32_t dim;
  std::memcpy(&dim, serialized.data(), sizeof(std::uint32_t));
  
  // Validate size
  std::size_t expected_size = sizeof(std::uint32_t) + dim * sizeof(float);
  if (serialized.size() != expected_size) {
    throw std::invalid_argument("Invalid serialized vector: size mismatch");
  }
  
  // Read float data
  Vector vec(dim);
  std::memcpy(vec.data_.data(), serialized.data() + sizeof(std::uint32_t), dim * sizeof(float));
  
  return vec;
}

// ====== Vector Operations ======

void Vector::Normalize() {
  float mag = Magnitude();
  if (mag > 0.0f) {
    for (auto& val : data_) {
      val /= mag;
    }
  }
}

float Vector::Magnitude() const {
  float sum_sq = 0.0f;
  for (float val : data_) {
    sum_sq += val * val;
  }
  return std::sqrt(sum_sq);
}

// ====== Distance Metrics ======

float ComputeDistance(const Vector& a, const Vector& b, DistanceMetric metric) {
  if (a.dimension() != b.dimension()) {
    throw std::invalid_argument("Vector dimensions must match for distance calculation");
  }
  
  switch (metric) {
    case DistanceMetric::kCosine:
      return CosineDistance(a, b);
    case DistanceMetric::kEuclidean:
      return EuclideanDistance(a, b);
    case DistanceMetric::kDotProduct:
      return DotProductDistance(a, b);
    case DistanceMetric::kManhattan:
      return ManhattanDistance(a, b);
    default:
      throw std::invalid_argument("Unknown distance metric");
  }
}

float CosineDistance(const Vector& a, const Vector& b) {
  return 1.0f - CosineSimilarity(a, b);
}

float EuclideanDistance(const Vector& a, const Vector& b) {
  float sum_sq = 0.0f;
  for (std::size_t i = 0; i < a.dimension(); ++i) {
    float diff = a[i] - b[i];
    sum_sq += diff * diff;
  }
  return std::sqrt(sum_sq);
}

float DotProductDistance(const Vector& a, const Vector& b) {
  // Negative dot product (so smaller = more similar for MIPS)
  return -DotProduct(a, b);
}

float ManhattanDistance(const Vector& a, const Vector& b) {
  float sum = 0.0f;
  for (std::size_t i = 0; i < a.dimension(); ++i) {
    sum += std::abs(a[i] - b[i]);
  }
  return sum;
}

float CosineSimilarity(const Vector& a, const Vector& b) {
  float dot = DotProduct(a, b);
  float mag_a = a.Magnitude();
  float mag_b = b.Magnitude();
  
  if (mag_a == 0.0f || mag_b == 0.0f) {
    return 0.0f;  // Undefined, return 0 to avoid division by zero
  }
  
  return dot / (mag_a * mag_b);
}

float DotProduct(const Vector& a, const Vector& b) {
  float sum = 0.0f;
  for (std::size_t i = 0; i < a.dimension(); ++i) {
    sum += a[i] * b[i];
  }
  return sum;
}

// ====== Utility Functions ======

DistanceMetric ParseDistanceMetric(const std::string& name) {
  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
  
  if (lower_name == "cosine") return DistanceMetric::kCosine;
  if (lower_name == "euclidean" || lower_name == "l2") return DistanceMetric::kEuclidean;
  if (lower_name == "dotproduct" || lower_name == "dot" || lower_name == "inner") return DistanceMetric::kDotProduct;
  if (lower_name == "manhattan" || lower_name == "l1") return DistanceMetric::kManhattan;
  
  throw std::invalid_argument("Unknown distance metric: " + name);
}

std::string ToString(DistanceMetric metric) {
  switch (metric) {
    case DistanceMetric::kCosine: return "cosine";
    case DistanceMetric::kEuclidean: return "euclidean";
    case DistanceMetric::kDotProduct: return "dotproduct";
    case DistanceMetric::kManhattan: return "manhattan";
    default: return "unknown";
  }
}

}  // namespace vector
}  // namespace core_engine
