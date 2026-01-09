#include <core_engine/vector/sift_parser.hpp>
#include <filesystem>
#include <iostream>

namespace core_engine {
namespace vector {

SiftParser::SiftParser(const std::string& filepath) : filepath_(filepath) {
}

SiftParser::~SiftParser() {
  Close();
}

bool SiftParser::Open() {
  file_.open(filepath_, std::ios::binary);
  if (!file_.is_open()) {
    return false;
  }

  // Peek at the first 4 bytes to get the dimension
  uint32_t dim;
  if (file_.read(reinterpret_cast<char*>(&dim), sizeof(uint32_t))) {
    dimension_ = dim;
    // Rewind to start
    file_.seekg(0, std::ios::beg);
  } else {
    file_.close();
    return false;
  }

  return true;
}

void SiftParser::Close() {
  if (file_.is_open()) {
    file_.close();
  }
}

std::optional<Vector> SiftParser::Next() {
  if (!IsGood())
    return std::nullopt;

  uint32_t dim;
  if (!file_.read(reinterpret_cast<char*>(&dim), sizeof(uint32_t))) {
    return std::nullopt; // EOF or error
  }

  if (dim == 0)
    return std::nullopt;
  if (dimension_ != 0 && dim != dimension_) {
    // Dimension mismatch in file
    return std::nullopt;
  }

  std::vector<float> data(dim);
  if (!file_.read(reinterpret_cast<char*>(data.data()), dim * sizeof(float))) {
    return std::nullopt;
  }

  return Vector(std::move(data));
}

size_t SiftParser::GetEstimatedTotal() const {
  if (dimension_ == 0)
    return 0;
  try {
    auto size = std::filesystem::file_size(filepath_);
    size_t record_size = sizeof(uint32_t) + dimension_ * sizeof(float);
    return size / record_size;
  } catch (...) {
    return 0;
  }
}

} // namespace vector
} // namespace core_engine
