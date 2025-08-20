#include "NDimensionalContainer.hpp"

namespace MayaFlux::Kakshya {

ContainerDataStructure::ContainerDataStructure(DataModality mod,
    OrganizationStrategy org,
    MemoryLayout layout)
    : modality(mod)
    , memory_layout(layout)
    , organization(org)
{
}

std::vector<DataDimension::Role> ContainerDataStructure::get_expected_dimension_roles() const
{
    switch (modality) {
    case DataModality::AUDIO_1D:
        return { DataDimension::Role::TIME };

    case DataModality::AUDIO_MULTICHANNEL:
        return { DataDimension::Role::TIME, DataDimension::Role::CHANNEL };

    case DataModality::IMAGE_2D:
        return { DataDimension::Role::SPATIAL_Y, DataDimension::Role::SPATIAL_X };

    case DataModality::IMAGE_COLOR:
        return { DataDimension::Role::SPATIAL_Y, DataDimension::Role::SPATIAL_X, DataDimension::Role::CHANNEL };

    case DataModality::VIDEO_GRAYSCALE:
        return { DataDimension::Role::TIME, DataDimension::Role::SPATIAL_Y, DataDimension::Role::SPATIAL_X };

    case DataModality::VIDEO_COLOR:
        return { DataDimension::Role::TIME, DataDimension::Role::SPATIAL_Y, DataDimension::Role::SPATIAL_X, DataDimension::Role::CHANNEL };

    case DataModality::SPECTRAL_2D:
        return { DataDimension::Role::TIME, DataDimension::Role::FREQUENCY };

    case DataModality::VOLUMETRIC_3D:
        return { DataDimension::Role::SPATIAL_X, DataDimension::Role::SPATIAL_Y, DataDimension::Role::SPATIAL_Z };

    case DataModality::TENSOR_ND:
    case DataModality::UNKNOWN:
    default:
        return {};
    }
}

ContainerDataStructure ContainerDataStructure::audio_planar()
{
    return { DataModality::AUDIO_MULTICHANNEL, OrganizationStrategy::PLANAR };
}

ContainerDataStructure ContainerDataStructure::audio_interleaved()
{
    return { DataModality::AUDIO_MULTICHANNEL, OrganizationStrategy::INTERLEAVED };
}

ContainerDataStructure ContainerDataStructure::image_planar()
{
    return { DataModality::IMAGE_COLOR, OrganizationStrategy::PLANAR };
}

ContainerDataStructure ContainerDataStructure::image_interleaved()
{
    return { DataModality::IMAGE_COLOR, OrganizationStrategy::INTERLEAVED };
}

size_t ContainerDataStructure::get_expected_variant_count(const std::vector<DataDimension>& dimensions) const
{
    if (organization == OrganizationStrategy::INTERLEAVED) {
        return 1;
    }

    switch (modality) {
    case DataModality::AUDIO_MULTICHANNEL:
    case DataModality::IMAGE_COLOR:
        return get_channel_count(dimensions);

    case DataModality::VIDEO_COLOR:
        return get_frame_count(dimensions) * get_channel_count(dimensions);

    default:
        return 1;
    }
}

bool ContainerDataStructure::validate_dimensions(const std::vector<DataDimension>& dimensions) const
{
    auto expected_roles = get_expected_dimension_roles();

    if (expected_roles.empty()) {
        return true;
    }

    if (dimensions.size() != expected_roles.size()) {
        return false;
    }

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role != expected_roles[i]) {
            return false;
        }
    }

    return true;
}

size_t ContainerDataStructure::get_dimension_index_for_role(const std::vector<DataDimension>& dimensions,
    DataDimension::Role role) const
{
    auto expected_roles = get_expected_dimension_roles();

    auto it = std::ranges::find(expected_roles, role);
    if (it != expected_roles.end()) {
        return std::distance(expected_roles.begin(), it);
    }

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == role) {
            return i;
        }
    }

    return SIZE_MAX;
}

size_t ContainerDataStructure::get_channel_count(const std::vector<DataDimension>& dimensions)
{
    for (const auto& dim : dimensions) {
        if (dim.role == DataDimension::Role::CHANNEL) {
            return dim.size;
        }
    }
    return 1;
}

size_t ContainerDataStructure::get_frame_count(const std::vector<DataDimension>& dimensions)
{
    for (const auto& dim : dimensions) {
        if (dim.role == DataDimension::Role::TIME) {
            return dim.size;
        }
    }
    return 1;
}

u_int64_t ContainerDataStructure::get_samples_count(const std::vector<DataDimension>& dimensions)
{
    for (const auto& dim : dimensions) {
        if (dim.role == DataDimension::Role::TIME) {
            return dim.size;
        }
    }
    return 0;
}

u_int64_t ContainerDataStructure::get_height(const std::vector<DataDimension>& dimensions)
{
    for (const auto& dim : dimensions) {
        if (dim.role == DataDimension::Role::SPATIAL_Y) {
            return dim.size;
        }
    }
    return 0;
}

u_int64_t ContainerDataStructure::get_width(const std::vector<DataDimension>& dimensions)
{
    for (const auto& dim : dimensions) {
        if (dim.role == DataDimension::Role::SPATIAL_X) {
            return dim.size;
        }
    }
    return 0;
}

}
