#pragma once

#include <core_engine/vector/vector.hpp>
#include <fstream>
#include <string>
#include <vector>
#include <optional>

namespace core_engine {
namespace vector {

/**
 * @brief Parser for SIFT datasets in .fvecs and .ivecs formats.
 * 
 * SIFT binary format (.fvecs):
 * For each vector:
 *   - 4 bytes (int32): dimension d
 *   - d * 4 bytes (float32): d vector components
 * 
 * Total file size = total_vectors * (4 + d * 4)
 */
class SiftParser {
public:
    explicit SiftParser(const std::string& filepath);
    ~SiftParser();

    // Prevent copying
    SiftParser(const SiftParser&) = delete;
    SiftParser& operator=(const SiftParser&) = delete;

    /**
     * @brief Open the file for reading.
     * @return true if successful, false otherwise.
     */
    bool Open();

    /**
     * @brief Close the file.
     */
    void Close();

    /**
     * @brief Read the next vector from the file.
     * @return The vector, or nullopt if EOF or error.
     */
    std::optional<Vector> Next();

    /**
     * @brief Get the dimension of vectors in this file.
     * Only valid after calling Open() and successfully reading at least once or peek.
     */
    uint32_t GetDimension() const { return dimension_; }

    /**
     * @brief Estimate total number of vectors based on file size.
     */
    size_t GetEstimatedTotal() const;

    /**
     * @brief Check if file is still good for reading.
     */
    bool IsGood() const { return file_.is_open() && file_.good(); }

private:
    std::string filepath_;
    std::ifstream file_;
    uint32_t dimension_ = 0;
};

} // namespace vector
} // namespace core_engine
