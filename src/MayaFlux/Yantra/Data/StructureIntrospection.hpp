#pragma once

#include "DataSpec.hpp"

namespace MayaFlux::Yantra {

/**
 * @brief Infer structure from DataVariant
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant(const Kakshya::DataVariant& data);

/**
 * @brief Infer structure from vector of DataVariants (NEW)
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant_vector(const std::vector<Kakshya::DataVariant>& data);

/**
 * @brief Infer structure from SignalSourceContainer
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_container(const std::shared_ptr<Kakshya::SignalSourceContainer>& container);

/**
 * @brief Infer structure from Region (placeholder since regions are markers)
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region, const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr);

/**
 * @brief Infer structure from RegionGroup
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group, const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr);

/**
 * @brief Infer structure from RegionSegments
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments, const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr);

/**
 * @brief Infer structure from Eigen matrix/vector
 */
template <typename EigenType>
static std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_eigen(const EigenType& eigen_data)
{
    std::vector<Kakshya::DataDimension> dimensions;
    Kakshya::DataModality modality {};

    using Scalar = typename EigenType::Scalar;

    if constexpr (EigenType::IsVectorAtCompileTime) {
        if constexpr (MayaFlux::ComplexData<Scalar>) {
            dimensions.emplace_back(Kakshya::DataDimension::frequency(eigen_data.size()));
            modality = Kakshya::DataModality::SPECTRAL_2D;
        } else if constexpr (MayaFlux::DecimalData<Scalar>) {
            dimensions.emplace_back(Kakshya::DataDimension::time(eigen_data.size()));
            modality = Kakshya::DataModality::AUDIO_1D;
        } else {
            dimensions.emplace_back(Kakshya::DataDimension("vector_data", eigen_data.size(), 1,
                Kakshya::DataDimension::Role::CUSTOM));
            modality = Kakshya::DataModality::TENSOR_ND;
        }
    } else {
        auto rows = eigen_data.rows();
        auto cols = eigen_data.cols();

        if constexpr (MayaFlux::ComplexData<Scalar>) {
            dimensions.emplace_back(Kakshya::DataDimension::time(rows, "time_frames"));
            dimensions.emplace_back(Kakshya::DataDimension::frequency(cols, "frequency_bins"));
            modality = Kakshya::DataModality::SPECTRAL_2D;
        } else if constexpr (MayaFlux::DecimalData<Scalar>) {
            dimensions.emplace_back(Kakshya::DataDimension::time(rows, "samples"));
            if (cols == 1) {
                modality = Kakshya::DataModality::AUDIO_1D;
            } else if (cols <= 16) {
                dimensions.emplace_back(Kakshya::DataDimension::channel(cols));
                modality = Kakshya::DataModality::AUDIO_MULTICHANNEL;
            } else {
                dimensions.emplace_back(Kakshya::DataDimension::frequency(cols, "features"));
                modality = Kakshya::DataModality::SPECTRAL_2D;
            }
        } else if constexpr (MayaFlux::IntegerData<Scalar>) {
            dimensions.emplace_back(Kakshya::DataDimension::spatial(rows, 'y'));
            dimensions.emplace_back(Kakshya::DataDimension::spatial(cols, 'x'));
            modality = Kakshya::DataModality::IMAGE_2D;
        } else {
            dimensions.emplace_back(Kakshya::DataDimension("matrix_rows", rows, 1,
                Kakshya::DataDimension::Role::CUSTOM));
            dimensions.emplace_back(Kakshya::DataDimension("matrix_cols", cols, 1,
                Kakshya::DataDimension::Role::CUSTOM));
            modality = Kakshya::DataModality::TENSOR_ND;
        }
    }

    return std::make_pair(std::move(dimensions), modality);
}

/**
 * @brief Generic structure inference for unknown types
 */
template <typename T>
static std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_generic_structure(const T& data)
{
    size_t size = 0;

    if constexpr (requires { data.size(); }) {
        size = data.size();
    } else if constexpr (requires { std::size(data); }) {
        size = std::size(data);
    } else {
        size = 1; // Single element
    }

    std::vector<Kakshya::DataDimension> dimensions;
    dimensions.emplace_back(Kakshya::DataDimension::time(size));

    return std::make_pair(std::move(dimensions), Kakshya::DataModality::TENSOR_ND);
}

/**
 * @brief Infer dimensions and modality from any ComputeData type
 * @tparam T ComputeData type
 * @param data Input data to analyze
 * @return Pair of (dimensions, modality)
 */
template <ComputeData T>
static std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_structure(const T& data, const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr)
{

    if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
        return infer_from_data_variant(data);
    } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
        return infer_from_container(data);
    } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
        return infer_from_region(data, container);
    } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
        return infer_from_region_group(data, container);
    } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
        return infer_from_segments(data, container);
    } else if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>) {
        return infer_from_eigen(data);
    } else {
        return infer_generic_structure(data);
    }
}

}
