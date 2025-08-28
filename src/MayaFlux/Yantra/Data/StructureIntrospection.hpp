#pragma once

#include "MayaFlux/Kakshya/Region.hpp"

#include "DataSpec.hpp"

namespace MayaFlux::Yantra {

/**
 * @brief Infer structure from DataVariant
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant(const Kakshya::DataVariant& data);

/**
 * @brief Infer structure from SignalSourceContainer
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_container(const std::shared_ptr<Kakshya::SignalSourceContainer>& container);

/**
 * @brief Infer structure from Region (placeholder since regions are markers)
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region);

/**
 * @brief Infer structure from RegionGroup
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group);

/**
 * @brief Infer structure from RegionSegments
 */
std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments);

/**
 * @brief Infer structure from Eigen matrix/vector
 */
template <typename EigenType>
static std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_eigen(const EigenType& eigen_data)
{

    std::vector<Kakshya::DataDimension> dimensions;
    Kakshya::DataModality modality {};

    if constexpr (EigenType::IsVectorAtCompileTime) {
        dimensions.emplace_back(Kakshya::DataDimension::time(eigen_data.size()));
        modality = Kakshya::DataModality::AUDIO_1D;
    } else {
        dimensions.emplace_back(Kakshya::DataDimension::time(eigen_data.rows(), "samples"));

        if (eigen_data.cols() > 1) {
            dimensions.emplace_back(Kakshya::DataDimension::channel(eigen_data.cols()));
            modality = (eigen_data.cols() == 2) ? Kakshya::DataModality::AUDIO_MULTICHANNEL : Kakshya::DataModality::TENSOR_ND;
        } else {
            modality = Kakshya::DataModality::AUDIO_1D;
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
infer_structure(const T& data)
{

    if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
        return infer_from_data_variant(data);
    } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
        return infer_from_container(data);
    } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
        return infer_from_region(data);
    } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
        return infer_from_region_group(data);
    } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
        return infer_from_segments(data);
    } else if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>) {
        return infer_from_eigen(data);
    } else {
        return infer_generic_structure(data);
    }
}

}
