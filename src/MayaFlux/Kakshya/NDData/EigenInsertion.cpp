#include "EigenInsertion.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace MayaFlux::Kakshya {

constexpr Eigen::Index EigenInsertion::get_expected_rows(MatrixInterpretation interp)
{
    switch (interp) {
    case MatrixInterpretation::SCALAR:
        return 1;
    case MatrixInterpretation::COMPLEX:
    case MatrixInterpretation::VEC2:
        return 2;
    case MatrixInterpretation::VEC3:
        return 3;
    case MatrixInterpretation::VEC4:
        return 4;
    case MatrixInterpretation::MAT4:
        return 16;
    case MatrixInterpretation::AUTO:
        return -1;
    }
    return -1;
}

void EigenInsertion::validate_matrix_dimensions(
    const Eigen::MatrixXd& matrix,
    MatrixInterpretation interpretation) const
{
    const auto expected_rows = get_expected_rows(interpretation);

    if (matrix.rows() != expected_rows) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "{} interpretation requires {} rows, but matrix has {} rows",
            Reflect::enum_to_string(interpretation),
            expected_rows,
            matrix.rows());
    }
}

void EigenInsertion::insert_vector(
    const Eigen::VectorXd& vec,
    bool preserve_precision)
{
    if (preserve_precision) {
        std::vector<double> data(vec.size());
        for (Eigen::Index i = 0; i < vec.size(); ++i) {
            data[i] = vec(i);
        }
        m_variant = Kakshya::DataVariant { std::move(data) };
    } else {
        std::vector<float> data(vec.size());
        for (Eigen::Index i = 0; i < vec.size(); ++i) {
            data[i] = static_cast<float>(vec(i));
        }
        m_variant = Kakshya::DataVariant { std::move(data) };
    }
}

void EigenInsertion::insert_matrix(
    const Eigen::MatrixXd& matrix,
    MatrixInterpretation interpretation,
    bool preserve_precision)
{
    if (interpretation == MatrixInterpretation::AUTO) {
        switch (matrix.rows()) {
        case 1:
            interpretation = MatrixInterpretation::SCALAR;
            break;
        case 2:
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Ambiguous 2-row matrix. Specify {} or {}",
                Reflect::enum_to_string(MatrixInterpretation::COMPLEX),
                Reflect::enum_to_string(MatrixInterpretation::VEC2));
            break;
        case 3:
            interpretation = MatrixInterpretation::VEC3;
            break;
        case 4:
            interpretation = MatrixInterpretation::VEC4;
            break;
        case 16:
            interpretation = MatrixInterpretation::MAT4;
            break;
        default:
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Cannot auto-interpret {}-row matrix. Specify MatrixInterpretation explicitly.",
                matrix.rows());
        }
    }

    validate_matrix_dimensions(matrix, interpretation);

    switch (interpretation) {
    case MatrixInterpretation::SCALAR:
        if (preserve_precision) {
            std::vector<double> data(matrix.cols());
            for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
                data[i] = matrix(0, i);
            }
            m_variant = Kakshya::DataVariant { std::move(data) };
        } else {
            std::vector<float> data(matrix.cols());
            for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
                data[i] = static_cast<float>(matrix(0, i));
            }
            m_variant = Kakshya::DataVariant { std::move(data) };
        }
        break;

    case MatrixInterpretation::COMPLEX:
        if (preserve_precision) {
            std::vector<std::complex<double>> data(matrix.cols());
            for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
                data[i] = std::complex<double>(matrix(0, i), matrix(1, i));
            }
            m_variant = Kakshya::DataVariant { std::move(data) };
        } else {
            std::vector<std::complex<float>> data(matrix.cols());
            for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
                data[i] = std::complex<float>(
                    static_cast<float>(matrix(0, i)),
                    static_cast<float>(matrix(1, i)));
            }
            m_variant = Kakshya::DataVariant { std::move(data) };
        }
        break;

    case MatrixInterpretation::VEC2: {
        std::vector<glm::vec2> data(matrix.cols());
        for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
            data[i] = glm::vec2(
                static_cast<float>(matrix(0, i)),
                static_cast<float>(matrix(1, i)));
        }
        m_variant = Kakshya::DataVariant { std::move(data) };
        break;
    }

    case MatrixInterpretation::VEC3: {
        std::vector<glm::vec3> data(matrix.cols());
        for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
            data[i] = glm::vec3(
                static_cast<float>(matrix(0, i)),
                static_cast<float>(matrix(1, i)),
                static_cast<float>(matrix(2, i)));
        }
        m_variant = Kakshya::DataVariant { std::move(data) };
        break;
    }

    case MatrixInterpretation::VEC4: {
        std::vector<glm::vec4> data(matrix.cols());
        for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
            data[i] = glm::vec4(
                static_cast<float>(matrix(0, i)),
                static_cast<float>(matrix(1, i)),
                static_cast<float>(matrix(2, i)),
                static_cast<float>(matrix(3, i)));
        }
        m_variant = Kakshya::DataVariant { std::move(data) };
        break;
    }

    case MatrixInterpretation::MAT4: {
        std::vector<glm::mat4> data(matrix.cols());
        for (Eigen::Index i = 0; i < matrix.cols(); ++i) {
            float mat_data[16];
            for (int j = 0; j < 16; ++j) {
                mat_data[j] = static_cast<float>(matrix(j, i));
            }
            data[i] = glm::make_mat4(mat_data);
        }
        m_variant = Kakshya::DataVariant { std::move(data) };
        break;
    }

    default:
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Invalid MatrixInterpretation: {}",
            Reflect::enum_to_string(interpretation));
    }
}

void EigenInsertion::clear()
{
    m_variant = Kakshya::DataVariant { std::vector<double> {} };
}

} // namespace MayaFlux::Kakshya
