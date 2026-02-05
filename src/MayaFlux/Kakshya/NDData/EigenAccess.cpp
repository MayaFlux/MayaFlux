#include "EigenAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace MayaFlux::Kakshya {

Eigen::VectorXd EigenAccess::to_vector() const
{
    return std::visit([this](const auto& vec) -> Eigen::VectorXd {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_arithmetic_v<T>) {
            return scalar_to_vector(vec);
        } else if constexpr (std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>) {
            Eigen::VectorXd result(vec.size());
            for (Eigen::Index i = 0; i < vec.size(); ++i) {
                result(i) = static_cast<double>(std::abs(vec[i]));
            }
            return result;
        } else if constexpr (GlmType<T>) {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Cannot convert structured GLM type {} to vector. Use to_matrix() instead.",
                typeid(T).name());
        } else {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Unsupported type for Eigen conversion: {}",
                typeid(T).name());
        }
    },
        m_variant);
}

Eigen::MatrixXd EigenAccess::to_matrix() const
{
    return std::visit([this](const auto& vec) -> Eigen::MatrixXd {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_arithmetic_v<T>) {
            return scalar_to_matrix(vec);
        } else if constexpr (std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>) {
            return complex_to_matrix(vec);
        } else if constexpr (GlmType<T>) {
            return glm_to_matrix(vec);
        } else {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Unsupported type for Eigen conversion: {}",
                typeid(T).name());
        }
    },
        m_variant);
}

Eigen::VectorXd EigenAccess::to_magnitude_vector() const
{
    if (!is_complex()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "to_magnitude_vector() requires complex data, but variant contains {}",
            type_name());
    }

    return std::visit([](const auto& vec) -> Eigen::VectorXd {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>) {
            Eigen::VectorXd result(vec.size());
            for (Eigen::Index i = 0; i < vec.size(); ++i) {
                result(i) = std::abs(vec[i]);
            }
            return result;
        } else {
            return Eigen::VectorXd(0);
        }
    },
        m_variant);
}

Eigen::VectorXcd EigenAccess::to_complex_vector() const
{
    if (!is_complex()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "to_complex_vector() requires complex data, but variant contains {}",
            type_name());
    }

    return std::visit([](const auto& vec) -> Eigen::VectorXcd {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_same_v<T, std::complex<float>>) {
            Eigen::VectorXcd result(vec.size());
            for (Eigen::Index i = 0; i < vec.size(); ++i) {
                result(i) = std::complex<double>(
                    static_cast<double>(vec[i].real()),
                    static_cast<double>(vec[i].imag()));
            }
            return result;
        } else if constexpr (std::is_same_v<T, std::complex<double>>) {
            Eigen::VectorXcd result(vec.size());
            for (Eigen::Index i = 0; i < vec.size(); ++i) {
                result(i) = vec[i];
            }
            return result;
        } else {
            return Eigen::VectorXcd(0);
        }
    },
        m_variant);
}

Eigen::VectorXd EigenAccess::to_phase_vector() const
{
    if (!is_complex()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "to_phase_vector() requires complex data, but variant contains {}",
            type_name());
    }

    return std::visit([](const auto& vec) -> Eigen::VectorXd {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>) {
            Eigen::VectorXd result(vec.size());
            for (Eigen::Index i = 0; i < vec.size(); ++i) {
                result(i) = std::arg(vec[i]); // atan2(imag, real)
            }
            return result;
        } else {
            return Eigen::VectorXd(0);
        }
    },
        m_variant);
}

size_t EigenAccess::element_count() const noexcept
{
    return std::visit([](const auto& vec) { return vec.size(); }, m_variant);
}

size_t EigenAccess::component_count() const
{
    return std::visit([](const auto& vec) -> size_t {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_arithmetic_v<T>) {
            return 1;
        } else if constexpr (std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>) {
            return 2;
        } else if constexpr (GlmVec2Type<T>) {
            return 2;
        } else if constexpr (GlmVec3Type<T>) {
            return 3;
        } else if constexpr (GlmVec4Type<T>) {
            return 4;
        } else if constexpr (GlmMatrixType<T>) {
            return 16;
        } else {
            return 1;
        }
    },
        m_variant);
}

std::pair<size_t, size_t> EigenAccess::matrix_dimensions() const
{
    return { component_count(), element_count() };
}

bool EigenAccess::validate_dimensions() const
{
    return std::visit([](const auto& vec) -> bool {
        return vec.size() <= std::numeric_limits<Eigen::Index>::max();
    },
        m_variant);
}

bool EigenAccess::is_complex() const
{
    return std::visit([](const auto& vec) -> bool {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        return std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>;
    },
        m_variant);
}

bool EigenAccess::is_structured() const
{
    return std::visit([](const auto& vec) -> bool {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        return GlmType<T>;
    },
        m_variant);
}

std::string EigenAccess::type_name() const
{
    return std::visit([](const auto& vec) -> std::string {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        return typeid(T).name();
    },
        m_variant);
}

template <typename T>
Eigen::VectorXd EigenAccess::scalar_to_vector(const std::vector<T>& vec) const
{
    if (vec.empty()) {
        return Eigen::VectorXd(0);
    }
    if (vec.size() > static_cast<size_t>(std::numeric_limits<Eigen::Index>::max())) {
        error<std::overflow_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Vector size {} exceeds Eigen::Index maximum {}",
            vec.size(),
            std::numeric_limits<Eigen::Index>::max());
    }

    Eigen::VectorXd result(vec.size());
    for (Eigen::Index i = 0; i < vec.size(); ++i) {
        result(i) = static_cast<double>(vec[i]);
    }
    return result;
}

template <typename T>
Eigen::MatrixXd EigenAccess::scalar_to_matrix(const std::vector<T>& vec) const
{
    Eigen::MatrixXd result(1, vec.size());
    for (Eigen::Index i = 0; i < vec.size(); ++i) {
        result(0, i) = static_cast<double>(vec[i]);
    }
    return result;
}

template <typename T>
Eigen::MatrixXd EigenAccess::complex_to_matrix(const std::vector<std::complex<T>>& vec) const
{
    Eigen::MatrixXd result(2, vec.size());
    for (Eigen::Index i = 0; i < vec.size(); ++i) {
        result(0, i) = static_cast<double>(vec[i].real());
        result(1, i) = static_cast<double>(vec[i].imag());
    }
    return result;
}

template <typename T>
Eigen::MatrixXd EigenAccess::glm_to_matrix(const std::vector<T>& vec) const
{
    if constexpr (GlmVec2Type<T>) {
        Eigen::MatrixXd result(2, vec.size());
        for (Eigen::Index i = 0; i < vec.size(); ++i) {
            result(0, i) = static_cast<double>(vec[i].x);
            result(1, i) = static_cast<double>(vec[i].y);
        }
        return result;
    } else if constexpr (GlmVec3Type<T>) {
        Eigen::MatrixXd result(3, vec.size());
        for (Eigen::Index i = 0; i < vec.size(); ++i) {
            result(0, i) = static_cast<double>(vec[i].x);
            result(1, i) = static_cast<double>(vec[i].y);
            result(2, i) = static_cast<double>(vec[i].z);
        }
        return result;
    } else if constexpr (GlmVec4Type<T>) {
        Eigen::MatrixXd result(4, vec.size());
        for (Eigen::Index i = 0; i < vec.size(); ++i) {
            result(0, i) = static_cast<double>(vec[i].x);
            result(1, i) = static_cast<double>(vec[i].y);
            result(2, i) = static_cast<double>(vec[i].z);
            result(3, i) = static_cast<double>(vec[i].w);
        }
        return result;
    } else if constexpr (GlmMatrixType<T>) {
        Eigen::MatrixXd result(16, vec.size());
        for (Eigen::Index i = 0; i < vec.size(); ++i) {
            const float* ptr = glm::value_ptr(vec[i]);
            for (int j = 0; j < 16; ++j) {
                result(j, i) = static_cast<double>(ptr[j]);
            }
        }
        return result;
    } else {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Unknown GLM type: {}",
            typeid(T).name());
    }
}

} // namespace MayaFlux::Kakshya
