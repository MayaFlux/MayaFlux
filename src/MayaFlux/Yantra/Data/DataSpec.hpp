#pragma once

#include "Eigen/Core"

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Yantra {

template <typename T>
concept VariantVector = requires {
    typename T::value_type;
    requires std::same_as<T, std::vector<typename T::value_type>>;
    requires std::constructible_from<Kakshya::DataVariant, typename T::value_type>;
};

// clang-format off

/**
 * @concept ComputeData
 * @brief Universal concept for types that can be used as data in compute operations.
 *
 * Defines the valid types that can flow through the computation pipeline.
 * Supports both direct type matching and constructible conversion patterns.
 *
 * Following types are considered valid:
 * - Kakshya::DataVariant: Universal data container
 * - std::shared_ptr<Kakshya::SignalSourceContainer>: Shared signal sources
 * - Kakshya::Region: Spatial/temporal markers
 * - Kakshya::RegionGroup: Collections of regions
 * - Eigen::MatrixBase<T>: Any Eigen matrix type (e.g., Vector3f, MatrixXd)
 * - std::vector<Kakshya::RegionSegment>: Cached region data (with values)
 * - Any type constructible from Kakshya::DataVariant
 */
template <typename T>
concept ComputeData =
    std::same_as<T, Kakshya::DataVariant> ||
    std::same_as<T, std::vector<Kakshya::DataVariant>> ||
    std::same_as<T, std::shared_ptr<Kakshya::SignalSourceContainer>> ||
    std::same_as<T, Kakshya::Region> ||
    std::same_as<T, Kakshya::RegionGroup> ||
    std::same_as<T, std::vector<Kakshya::RegionSegment>> ||
    std::is_base_of_v<Eigen::MatrixBase<T>, T> ||
    VariantVector<T> ||
    std::constructible_from<Kakshya::DataVariant, T>;
// clang-format on

/**
 * @struct extraction_traits_d
 * @brief Traits to determine how to extract data from various types. The returned values are alwayu doubles (_d)
 *
 * This struct provides compile-time information about how to handle different data types
 * in terms of whether they represent multiple variants, require a container, or are region-like.
 * It also defines the expected result types for data extraction.
 *
 * Specializations are provided for:
 * - Kakshya::DataVariant
 * - std::vector<Kakshya::DataVariant>
 * - std::shared_ptr<Kakshya::SignalSourceContainer>
 * - Kakshya::Region
 * - Kakshya::RegionGroup
 * - std::vector<Kakshya::RegionSegment>
 * - Eigen::Matrix and its specializations (e.g., Eigen::VectorXd, Eigen::MatrixXd)
 */
template <typename T>
struct extraction_traits_d {
    static constexpr bool is_multi_variant = false;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::span<double>;
    using variant_result_type = Kakshya::DataVariant;
};

// Helper concepts for easier SFINAE and static_assert usage
template <typename T>
concept RegionLike = std::is_same_v<T, Kakshya::Region>
    || std::is_same_v<T, Kakshya::RegionGroup>
    || std::is_same_v<T, std::vector<Kakshya::RegionSegment>>;

// A MultiVariant is either a vector of DataVariants, a shared_ptr to a SignalSourceContainer, or any RegionLike type
template <typename T>
concept MultiVariant = std::is_same_v<T, std::vector<Kakshya::DataVariant>>
    || std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>
    || RegionLike<T>;

// A RequiresContainer is any RegionLike type
template <typename T>
concept RequiresContainer = RegionLike<T>;

/**
 * @brief A SingleVariant is either a single DataVariant, an Eigen vector type, or any type constructible from DataVariant
 * but not a vector of DataVariants, not a shared_ptr to SignalSourceContainer, and not RegionLike.
 *
 * This concept ensures that the type represents a single data source rather than multiple sources.
 */
template <typename T>
concept SingleVariant = std::is_same_v<T, Kakshya::DataVariant>
    || (std::is_base_of_v<Eigen::MatrixBase<T>, T> && T::ColsAtCompileTime == 1)
    || std::is_same_v<T, Eigen::VectorXd>
    || (std::constructible_from<Kakshya::DataVariant, T>
        && !std::is_same_v<T, std::vector<Kakshya::DataVariant>>
        && !std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>
        && !RegionLike<T>
        && !std::is_base_of_v<Eigen::MatrixBase<T>, T>);

/**
 * @brief Specialization of extraction_traits_d for single Kakshya::DataVariant
 */
template <>
struct extraction_traits_d<Kakshya::DataVariant> {
    static constexpr bool is_multi_variant = false;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::span<double>;
    using variant_result_type = Kakshya::DataVariant;
};

/**
 * @brief Specialization of extraction_traits_d for vector of Kakshya::DataVariant
 */
template <>
struct extraction_traits_d<std::vector<Kakshya::DataVariant>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @brief Specialization of extraction_traits_d for shared_ptr to Kakshya::SignalSourceContainer
 */
template <>
struct extraction_traits_d<std::shared_ptr<Kakshya::SignalSourceContainer>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @brief Specialization of extraction_traits_d for Kakshya::Region
 */
template <>
struct extraction_traits_d<Kakshya::Region> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = true;
    static constexpr bool is_region_like = true;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @brief Specialization of extraction_traits_d for Kakshya::RegionGroup
 */
template <>
struct extraction_traits_d<Kakshya::RegionGroup> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = true;
    static constexpr bool is_region_like = true;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @brief Specialization of extraction_traits_d for vector of Kakshya::RegionSegment
 */
template <>
struct extraction_traits_d<std::vector<Kakshya::RegionSegment>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = true;
    static constexpr bool is_region_like = true;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

// Trait to detect if a type is an Eigen matrix
template <typename T>
struct is_eigen_matrix : std::false_type { };
template <typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
struct is_eigen_matrix<Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>> : std::true_type { };
template <typename T>
inline constexpr bool is_eigen_matrix_v = is_eigen_matrix<T>::value;

/**
 * @brief Specialization of extraction_traits_d for Eigen matrices with double scalar type
 *
 * This specialization ensures that Eigen matrices with double precision are treated as multi-variant types.
 */
template <typename T>
    requires is_eigen_matrix_v<T> && std::is_same_v<typename T::Scalar, double>
struct extraction_traits_d<T> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @brief Specialization of extraction_traits_d for Eigen::MatrixXd
 *
 * This specialization handles Eigen matrices, treating them as multi-variant types.
 */
template <>
struct extraction_traits_d<Eigen::MatrixXd> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @concept EigenMatrixLike
 * @brief Concept for Eigen matrix types with double scalar
 */
template <typename T>
concept EigenMatrixLike = is_eigen_matrix_v<T> && std::is_same_v<typename T::Scalar, double>;

template <typename T>
using enable_if_single_variant_t = std::enable_if_t<SingleVariant<T>>;

template <typename T>
using enable_if_multi_variant_t = std::enable_if_t<extraction_traits_d<T>::is_multi_variant>;

template <typename T>
using enable_if_region_like_t = std::enable_if_t<extraction_traits_d<T>::is_region_like>;

template <typename T>
using enable_if_multi_no_container_t = std::enable_if_t<extraction_traits_d<T>::is_multi_variant && !extraction_traits_d<T>::requires_container>;

template <typename T>
using enable_if_multi_with_container_t = std::enable_if_t<extraction_traits_d<T>::is_multi_variant && extraction_traits_d<T>::requires_container>;

template <typename T>
using enable_if_eigen_matrix_t = std::enable_if_t<is_eigen_matrix_v<T> && std::is_same_v<typename T::Scalar, double>>;

template <typename T>
using extraction_result_t = typename extraction_traits_d<T>::result_type;

template <typename T>
using variant_result_t = typename extraction_traits_d<T>::variant_result_type;

}
