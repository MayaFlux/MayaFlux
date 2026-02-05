#pragma once

#include "NDData.hpp"

#include <Eigen/Dense>

namespace MayaFlux::Kakshya {

/**
 * @class EigenAccess
 * @brief Type-erased accessor for converting DataVariant to Eigen types
 *
 * Provides semantic, easy-to-follow conversion from NDData to Eigen.
 * Companion to EigenInsertion for write operations.
 *
 * Design principle: All numeric types convert cleanly to Eigen's double representation.
 * Complex types become 2-row matrices (real, imaginary).
 */
class MAYAFLUX_API EigenAccess {
public:
    explicit EigenAccess(const Kakshya::DataVariant& variant)
        : m_variant(variant)
    {
    }

    /**
     * @brief Convert DataVariant to Eigen column vector
     * @return Vector where each element is a sample
     *
     * For scalar data: direct conversion
     * For complex data: magnitude by default (use to_matrix() for real/imag separation)
     */
    [[nodiscard]] Eigen::VectorXd to_vector() const;

    /**
     * @brief Convert DataVariant to Eigen matrix
     * @return Matrix representation
     *
     * - Scalar types: 1xN matrix (single row)
     * - Complex types: 2xN matrix (row 0 = real, row 1 = imag)
     * - GLM vec2: 2xN matrix
     * - GLM vec3: 3xN matrix
     * - GLM vec4: 4xN matrix
     * - GLM mat4: 16xN matrix (flattened)
     */
    [[nodiscard]] Eigen::MatrixXd to_matrix() const;

    /**
     * @brief Convert complex data to magnitude vector
     * @return Vector of magnitudes (|z| = √(real² + imag²))
     * @throws std::invalid_argument if not complex data
     */
    [[nodiscard]] Eigen::VectorXd to_magnitude_vector() const;

    /**
     * @brief Convert complex data to Eigen complex vector
     * @return Complex vector (native Eigen representation)
     * @throws std::invalid_argument if not complex data
     */
    [[nodiscard]] Eigen::VectorXcd to_complex_vector() const;

    /**
     * @brief Convert complex data to phase vector (angle in radians)
     * @return Vector of phases (∠z = atan2(imag, real))
     * @throws std::invalid_argument if not complex data
     */
    [[nodiscard]] Eigen::VectorXd to_phase_vector() const;

    /**
     * @brief Get number of elements (columns in matrix representation)
     * @return Element count
     */
    [[nodiscard]] size_t element_count() const noexcept;

    /**
     * @brief Get number of components per element (rows in matrix representation)
     * @return Component count (1 for scalar, 2 for complex/vec2, 3 for vec3, etc.)
     */
    [[nodiscard]] size_t component_count() const;

    /**
     * @brief Get matrix dimensions as (rows, cols)
     * @return Pair of (component_count, element_count)
     */
    [[nodiscard]] std::pair<size_t, size_t> matrix_dimensions() const;

    /**
     * @brief Validate that data dimensions fit within Eigen limits
     * @return true if valid
     */
    [[nodiscard]] bool validate_dimensions() const;

    /**
     * @brief Check if data contains complex numbers
     * @return true if complex data
     */
    [[nodiscard]] bool is_complex() const;

    /**
     * @brief Check if data contains GLM types
     * @return true if GLM structured data
     */
    [[nodiscard]] bool is_structured() const;

    /**
     * @brief Get underlying data type name
     * @return String description of type
     */
    [[nodiscard]] std::string type_name() const;

    /**
     * @brief Get zero-copy Eigen::Map view of data
     * @tparam EigenType Target Eigen type (VectorXd, VectorXf, VectorXcd, VectorXcf)
     * @return Optional Eigen::Map pointing to variant's internal memory
     *
     * Returns std::nullopt if:
     * - Type mismatch (e.g., vector<float> → VectorXd requires conversion)
     * - Data is empty
     *
     * Supported zero-copy views:
     * - vector<double> → Map<VectorXd>
     * - vector<float> → Map<VectorXf>
     * - vector<complex<double>> → Map<VectorXcd>
     * - vector<complex<float>> → Map<VectorXcf>
     */
    template <typename EigenType>
    auto view() const -> std::optional<Eigen::Map<const EigenType>>;

    /**
     * @brief Get zero-copy matrix view with explicit row count
     * @tparam Scalar Element type (double or float)
     * @param rows Number of rows in matrix interpretation
     * @return Optional Eigen::Map as rowsxcols matrix (row-major)
     *
     * Returns std::nullopt if:
     * - Type mismatch
     * - Data size not divisible by rows
     * - Data is empty
     *
     * Example: vector<double> with 12 elements, rows=3 → 3x4 matrix
     */
    template <typename Scalar = double>
    auto view_as_matrix(Eigen::Index rows) const
        -> std::optional<Eigen::Map<const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>;

    /**
     * @brief Get zero-copy span view of GLM vector data
     * @tparam GlmVecType GLM type (glm::vec2, glm::vec3, glm::vec4)
     * @return Optional span pointing to variant's internal memory
     *
     * Returns std::nullopt if variant doesn't contain exactly GlmVecType
     */
    template <typename GlmVecType>
        requires GlmType<GlmVecType>
    auto view_as_glm() const -> std::optional<std::span<const GlmVecType>>;

private:
    const Kakshya::DataVariant& m_variant;

    template <typename T>
    Eigen::VectorXd scalar_to_vector(const std::vector<T>& vec) const;

    template <typename T>
    Eigen::MatrixXd scalar_to_matrix(const std::vector<T>& vec) const;

    template <typename T>
    Eigen::MatrixXd complex_to_matrix(const std::vector<std::complex<T>>& vec) const;

    template <typename T>
    Eigen::MatrixXd glm_to_matrix(const std::vector<T>& vec) const;
};

/**
 * @brief Convenience function for direct conversion
 * @param variant Input DataVariant
 * @return Eigen matrix representation
 */
inline Eigen::MatrixXd to_eigen_matrix(const Kakshya::DataVariant& variant)
{
    return EigenAccess(variant).to_matrix();
}

/**
 * @brief Convenience function for vector conversion
 * @param variant Input DataVariant
 * @return Eigen vector representation
 */
inline Eigen::VectorXd to_eigen_vector(const Kakshya::DataVariant& variant)
{
    return EigenAccess(variant).to_vector();
}

template <typename EigenType>
auto EigenAccess::view() const -> std::optional<Eigen::Map<const EigenType>>
{
    return std::visit([](const auto& vec) -> std::optional<Eigen::Map<const EigenType>> {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if (vec.empty()) {
            return std::nullopt;
        }

        if constexpr (std::is_same_v<EigenType, Eigen::VectorXd>) {
            if constexpr (std::is_same_v<T, double>) {
                return Eigen::Map<const Eigen::VectorXd>(vec.data(), vec.size());
            }
        } else if constexpr (std::is_same_v<EigenType, Eigen::VectorXf>) {
            if constexpr (std::is_same_v<T, float>) {
                return Eigen::Map<const Eigen::VectorXf>(vec.data(), vec.size());
            }
        } else if constexpr (std::is_same_v<EigenType, Eigen::VectorXcd>) {
            if constexpr (std::is_same_v<T, std::complex<double>>) {
                return Eigen::Map<const Eigen::VectorXcd>(
                    reinterpret_cast<const std::complex<double>*>(vec.data()),
                    vec.size());
            }
        } else if constexpr (std::is_same_v<EigenType, Eigen::VectorXcf>) {
            if constexpr (std::is_same_v<T, std::complex<float>>) {
                return Eigen::Map<const Eigen::VectorXcf>(
                    reinterpret_cast<const std::complex<float>*>(vec.data()),
                    vec.size());
            }
        }

        return std::nullopt;
    },
        m_variant);
}

template <typename Scalar>
auto EigenAccess::view_as_matrix(Eigen::Index rows) const
    -> std::optional<Eigen::Map<const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>>
{
    return std::visit([rows](const auto& vec)
                          -> std::optional<Eigen::Map<const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>> {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if (vec.empty()) {
            return std::nullopt;
        }

        if constexpr (std::is_same_v<T, Scalar>) {
            if (vec.size() % rows != 0) {
                return std::nullopt;
            }

            Eigen::Index cols = vec.size() / rows;
            return Eigen::Map<const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(
                vec.data(), rows, cols);
        }

        return std::nullopt;
    },
        m_variant);
}

template <typename GlmVecType>
    requires GlmType<GlmVecType>
auto EigenAccess::view_as_glm() const -> std::optional<std::span<const GlmVecType>>
{
    return std::visit([](const auto& vec) -> std::optional<std::span<const GlmVecType>> {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_same_v<T, GlmVecType>) {
            return std::span<const GlmVecType>(vec.data(), vec.size());
        }

        return std::nullopt;
    },
        m_variant);
}

} // namespace MayaFlux::Kakshya
