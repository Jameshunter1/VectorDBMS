#pragma once

#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace core_engine {

// Simple RAII wrapper for sector-aligned buffers required by direct I/O.
class AlignedBuffer {
public:
  explicit AlignedBuffer(std::size_t size, std::size_t alignment = 4096)
      : data_(nullptr), size_(size), alignment_(alignment) {
    if (alignment_ == 0 || (alignment_ & (alignment_ - 1)) != 0) {
      throw std::invalid_argument("Alignment must be a power of two");
    }
    Allocate();
  }

  ~AlignedBuffer() {
    Release();
  }

  AlignedBuffer(const AlignedBuffer&) = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;

  AlignedBuffer(AlignedBuffer&& other) noexcept
      : data_(other.data_), size_(other.size_), alignment_(other.alignment_) {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  AlignedBuffer& operator=(AlignedBuffer&& other) noexcept {
    if (this != &other) {
      Release();
      data_ = other.data_;
      size_ = other.size_;
      alignment_ = other.alignment_;
      other.data_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  void* data() {
    return data_;
  }

  const void* data() const {
    return data_;
  }

  template <typename T> T* as() {
    return static_cast<T*>(data_);
  }

  template <typename T> const T* as() const {
    return static_cast<const T*>(data_);
  }

  std::size_t size() const {
    return size_;
  }

  std::size_t alignment() const {
    return alignment_;
  }

private:
  void Allocate() {
#if defined(_WIN32)
    data_ = _aligned_malloc(size_, alignment_);
    if (!data_) {
      throw std::bad_alloc();
    }
#else
    if (posix_memalign(&data_, alignment_, size_) != 0) {
      data_ = nullptr;
      throw std::bad_alloc();
    }
#endif
  }

  void Release() {
    if (!data_) {
      return;
    }
#if defined(_WIN32)
    _aligned_free(data_);
#else
    std::free(data_);
#endif
    data_ = nullptr;
  }

  void* data_;
  std::size_t size_;
  std::size_t alignment_;
};

} // namespace core_engine
