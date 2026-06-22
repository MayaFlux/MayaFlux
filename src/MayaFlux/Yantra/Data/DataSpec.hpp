#pragma once

#include "Eigen/Core"

#include "MayaFlux/Kakshya/Region/RegionSegment.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Yantra {

// =============================================================================
// VariantVector concept
// =============================================================================

template <typename T>
concept VariantVector = requires {
    typename T::value_type;
    requires std::same_as<T, std::vector<typename T::value_type>>;
    requires std::constructible_from<Kakshya::DataVariant, typename T::value_type>;
};

// =============================================================================
// Eigen matrix trait
// =============================================================================

template <typename T>
struct is_eigen_matrix : std::false_type { };

template <typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
struct is_eigen_matrix<Eigen::Matrix<Scalar, Rows, Cols, Options, MaxRows, MaxCols>> : std::true_type { };

template <typename T>
inline constexpr bool is_eigen_matrix_v = is_eigen_matrix<T>::value;

// =============================================================================
// ComputeData concept
// =============================================================================

/**
 * @concept ComputeData
 * @brief Universal concept for types that can flow through the computation pipeline.
 *
 * Valid types:
 * - Kakshya::DataVariant
 * - std::vector<Kakshya::DataVariant>
 * - std::shared_ptr<Kakshya::SignalSourceContainer>
 * - Kakshya::Region
 * - Kakshya::RegionGroup
 * - std::vector<Kakshya::RegionSegment>
 * - Any Eigen matrix type (any scalar)
 * - Any VariantVector (std::vector<T> where T is constructible into DataVariant)
 * - Any type constructible from Kakshya::DataVariant
 */
// clang-format off
template <typename T>
concept ComputeData =
    std::same_as<T, Kakshya::DataVariant>                          ||
    std::same_as<T, std::vector<Kakshya::DataVariant>>             ||
    std::same_as<T, std::shared_ptr<Kakshya::SignalSourceContainer>> ||
    std::same_as<T, Kakshya::Region>                               ||
    std::same_as<T, Kakshya::RegionGroup>                          ||
    std::same_as<T, std::vector<Kakshya::RegionSegment>>           ||
    std::is_base_of_v<Eigen::MatrixBase<T>, T>                     ||
    VariantVector<T>                                                ||
    std::constructible_from<Kakshya::DataVariant, T>;
// clang-format on

// =============================================================================
// Structural concepts
// =============================================================================

/**
 * @concept RegionLike
 * @brief Types that represent spatial or temporal markers requiring a container to resolve data.
 */
template <typename T>
concept RegionLike = std::is_same_v<T, Kakshya::Region> || std::is_same_v<T, Kakshya::RegionGroup> || std::is_same_v<T, std::vector<Kakshya::RegionSegment>>;

/**
 * @concept MultiVariant
 * @brief Types that yield multiple data channels on extraction.
 */
template <typename T>
concept MultiVariant = std::is_same_v<T, std::vector<Kakshya::DataVariant>> || std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>> || RegionLike<T>;

/**
 * @concept RequiresContainer
 * @brief Types that need an associated SignalSourceContainer to extract data.
 */
template <typename T>
concept RequiresContainer = RegionLike<T>;

/**
 * @concept EigenMatrixLike
 * @brief Any Eigen matrix type, regardless of scalar type.
 *
 * The previous definition gated on Scalar == double, rejecting MatrixXf,
 * Matrix3f, VectorXf, and all other non-double Eigen types. Widening to
 * double for algorithm input is the algorithm's concern, not the concept's.
 */
template <typename T>
concept EigenMatrixLike = is_eigen_matrix_v<T>;

/**
 * @concept SingleVariant
 * @brief Single data source: one DataVariant, a column Eigen vector of any
 *        scalar type, or any type constructible from DataVariant that is not
 *        multi-variant or region-like.
 */
template <typename T>
concept SingleVariant = std::is_same_v<T, Kakshya::DataVariant>
    || (is_eigen_matrix_v<T> && T::ColsAtCompileTime == 1)
    || (std::constructible_from<Kakshya::DataVariant, T>
        && !std::is_same_v<T, std::vector<Kakshya::DataVariant>>
        && !std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>
        && !RegionLike<T>
        && !is_eigen_matrix_v<T>);

// =============================================================================
// extraction_traits_d -- algorithm (double) extraction path
//
// The _d suffix is intentional: these traits describe extraction for Kinesis
// algorithm input, which always operates on double-precision spans. All
// existing callers (analyzers, sorters, transformers) use this path unchanged.
// =============================================================================

/**
 * @struct extraction_traits_d
 * @brief Compile-time traits describing how to extract double-precision data
 *        from a given ComputeData type for use by Kinesis algorithms.
 *
 * result_type is always span<double> (single) or vector<span<double>> (multi).
 * Widening from native types is performed at the extraction site.
 */
template <typename T>
struct extraction_traits_d {
    static constexpr bool is_multi_variant = false;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::span<double>;
    using variant_result_type = Kakshya::DataVariant;
};

template <>
struct extraction_traits_d<Kakshya::DataVariant> {
    static constexpr bool is_multi_variant = false;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::span<double>;
    using variant_result_type = Kakshya::DataVariant;
};

template <>
struct extraction_traits_d<std::vector<Kakshya::DataVariant>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

template <>
struct extraction_traits_d<std::shared_ptr<Kakshya::SignalSourceContainer>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

template <>
struct extraction_traits_d<Kakshya::Region> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = true;
    static constexpr bool is_region_like = true;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

template <>
struct extraction_traits_d<Kakshya::RegionGroup> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = true;
    static constexpr bool is_region_like = true;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

template <>
struct extraction_traits_d<std::vector<Kakshya::RegionSegment>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = true;
    static constexpr bool is_region_like = true;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

/**
 * @brief extraction_traits_d for any Eigen matrix type.
 *
 * Covers MatrixXd, MatrixXf, VectorXf, Matrix3f, etc. result_type remains
 * vector<span<double>> because Kinesis algorithms require double input;
 * widening from T::Scalar happens at the extraction site.
 */
template <typename T>
    requires is_eigen_matrix_v<T>
struct extraction_traits_d<T> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using result_type = std::vector<std::span<double>>;
    using variant_result_type = std::vector<Kakshya::DataVariant>;
};

// =============================================================================
// extraction_traits -- native-type aware companion to extraction_traits_d
//
// Carries the same structural flags but also exposes:
//   native_element_type  -- the scalar the source actually stores.
//   native_result_type   -- zero-copy span in native type.
//   algorithm_result_type -- double span for Kinesis algorithm input.
//
// Use this when staging GPU buffers, roundtripping containers, or any path
// that should not force conversion to double.
// =============================================================================

namespace detail {

    template <typename T>
    struct native_element {
        using type = double;
    };

    template <>
    struct native_element<std::vector<Kakshya::DataVariant>> {
        using type = void; ///< Heterogeneous -- visit the variant at runtime.
    };

    template <>
    struct native_element<std::shared_ptr<Kakshya::SignalSourceContainer>> {
        using type = void; ///< Runtime-determined via value_element_type().
    };

    template <typename T>
        requires is_eigen_matrix_v<T>
    struct native_element<T> {
        using type = typename T::Scalar;
    };

    template <typename T>
        requires RegionLike<T>
    struct native_element<T> {
        using type = double;
    };

} // namespace detail

/**
 * @struct extraction_traits
 * @brief Native-type aware extraction traits.
 *
 * extraction_traits_d and its aliases are unchanged; this struct is purely
 * additive. Callers that need native access (GPU staging, container
 * roundtrips) use extraction_traits<T> or the native_result_t / native_element_t
 * convenience aliases.
 */
template <typename T>
struct extraction_traits {
    static constexpr bool is_multi_variant = extraction_traits_d<T>::is_multi_variant;
    static constexpr bool requires_container = extraction_traits_d<T>::requires_container;
    static constexpr bool is_region_like = extraction_traits_d<T>::is_region_like;

    using native_element_type = typename detail::native_element<T>::type;
    using algorithm_result_type = typename extraction_traits_d<T>::result_type;

    using native_result_type = std::conditional_t<
        std::is_same_v<native_element_type, void>,
        algorithm_result_type,
        std::conditional_t<
            extraction_traits_d<T>::is_multi_variant,
            std::vector<std::span<native_element_type>>,
            std::span<native_element_type>>>;
};

template <>
struct extraction_traits<Kakshya::DataVariant> {
    static constexpr bool is_multi_variant = false;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using native_element_type = void;
    using algorithm_result_type = std::span<double>;
    using native_result_type = algorithm_result_type;
};

template <>
struct extraction_traits<std::shared_ptr<Kakshya::SignalSourceContainer>> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using native_element_type = void;
    using algorithm_result_type = std::vector<std::span<double>>;
    using native_result_type = algorithm_result_type;
};

template <typename T>
    requires is_eigen_matrix_v<T>
struct extraction_traits<T> {
    static constexpr bool is_multi_variant = true;
    static constexpr bool requires_container = false;
    static constexpr bool is_region_like = false;
    using native_element_type = typename T::Scalar;
    using algorithm_result_type = std::vector<std::span<double>>;
    using native_result_type = std::vector<std::span<native_element_type>>;
};

// =============================================================================
// enable_if aliases -- SFINAE helpers
// =============================================================================

template <typename T>
using enable_if_single_variant_t = std::enable_if_t<SingleVariant<T>>;

template <typename T>
using enable_if_multi_variant_t = std::enable_if_t<extraction_traits_d<T>::is_multi_variant>;

template <typename T>
using enable_if_region_like_t = std::enable_if_t<extraction_traits_d<T>::is_region_like>;

template <typename T>
using enable_if_multi_no_container_t = std::enable_if_t<
    extraction_traits_d<T>::is_multi_variant && !extraction_traits_d<T>::requires_container>;

template <typename T>
using enable_if_multi_with_container_t = std::enable_if_t<
    extraction_traits_d<T>::is_multi_variant && extraction_traits_d<T>::requires_container>;

template <typename T>
using enable_if_eigen_matrix_t = std::enable_if_t<is_eigen_matrix_v<T>>;

// =============================================================================
// Type aliases
// =============================================================================

/// Algorithm (double) result type for T -- for Kinesis callers.
template <typename T>
using extraction_result_t = typename extraction_traits_d<T>::result_type;

/// Variant result type for T.
template <typename T>
using variant_result_t = typename extraction_traits_d<T>::variant_result_type;

/// Native result type for T -- zero-copy in the source's own scalar type.
template <typename T>
using native_result_t = typename extraction_traits<T>::native_result_type;

/// Native element type for T.
template <typename T>
using native_element_t = typename extraction_traits<T>::native_element_type;

} // namespace MayaFlux::Yantra
