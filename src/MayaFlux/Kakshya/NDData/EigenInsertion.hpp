#pragma once

#include "NDData.hpp"
#include <Eigen/Dense>

namespace MayaFlux::Kakshya {

/**
 * @enum MatrixInterpretation
 * @brief How to interpret Eigen matrix rows/columns
 */
enum class MatrixInterpretation : uint8_t {
    SCALAR, ///< Single row → scalar values
    COMPLEX, ///< 2 rows → complex (row 0 = real, row 1 = imag)
    VEC2, ///< 2 rows → glm::vec2
    VEC3, ///< 3 rows → glm::vec3
    VEC4, ///< 4 rows → glm::vec4
    MAT4, ///< 16 rows → glm::mat4 (flattened)
    AUTO ///< Infer from row count
};

/**
 * @class EigenInsertion
 * @brief Type-erased writer for converting Eigen types to DataVariant
 *
 * Companion to EigenAccess for write operations.
 * Provides semantic conversion from Eigen to NDData.
 *
 * Design principle: Matrix structure determines output type.
 * User can override interpretation for ambiguous cases.
 */
class MAYAFLUX_API EigenInsertion {
public:
    explicit EigenInsertion(Kakshya::DataVariant& variant)
        : m_variant(variant)
    {
    }

    /**
     * @brief Insert Eigen vector as scalar data
     * @param vec Source vector
     * @param preserve_precision If true, use double; if false, convert to float
     */
    void insert_vector(
        const Eigen::VectorXd& vec,
        bool preserve_precision = true);

    /**
     * @brief Insert Eigen matrix with automatic interpretation
     * @param matrix Source matrix (columns are data points)
     * @param interpretation How to interpret matrix structure
     * @param preserve_precision If true, use double for scalars
     *
     * AUTO interpretation rules:
     * - 1 row → scalar (double or float based on preserve_precision)
     * - 2 rows → complex<double> or glm::vec2 (use interpretation to specify)
     * - 3 rows → glm::vec3
     * - 4 rows → glm::vec4
     * - 16 rows → glm::mat4
     * - Other → error (must specify interpretation)
     */
    void insert_matrix(
        const Eigen::MatrixXd& matrix,
        MatrixInterpretation interpretation = MatrixInterpretation::AUTO,
        bool preserve_precision = true);

    /**
     * @brief Clear variant data
     */
    void clear();

private:
    Kakshya::DataVariant& m_variant;

    static constexpr Eigen::Index get_expected_rows(MatrixInterpretation interp);

    void validate_matrix_dimensions(
        const Eigen::MatrixXd& matrix,
        MatrixInterpretation interpretation) const;
};

/**
 * @brief Convenience function for direct conversion
 * @param matrix Input Eigen matrix
 * @param interpretation How to interpret structure
 * @return DataVariant representation
 */
inline Kakshya::DataVariant from_eigen_matrix(
    const Eigen::MatrixXd& matrix,
    MatrixInterpretation interpretation = MatrixInterpretation::AUTO)
{
    Kakshya::DataVariant variant;
    EigenInsertion insertion(variant);
    insertion.insert_matrix(matrix, interpretation);
    return variant;
}

/**
 * @brief Convenience function for vector conversion
 * @param vec Input Eigen vector
 * @return DataVariant representation (vector<double>)
 */
inline Kakshya::DataVariant from_eigen_vector(const Eigen::VectorXd& vec)
{
    Kakshya::DataVariant variant;
    EigenInsertion insertion(variant);
    insertion.insert_vector(vec);
    return variant;
}

} // namespace MayaFlux::Kakshya
