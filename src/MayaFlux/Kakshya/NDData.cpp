#include "NDData.hpp"

namespace MayaFlux::Kakshya {

DataDimension::DataDimension(std::string n, u_int64_t s, u_int64_t st, Role r)
    : name(std::move(n))
    , size(s)
    , stride(st)
    , role(r)
{
}

DataDimension DataDimension::time(u_int64_t samples, std::string name)
{
    return { std::move(name), samples, 1, Role::TIME };
}

DataDimension DataDimension::channel(u_int64_t count, u_int64_t stride)
{
    return { "channel", count, stride, Role::CHANNEL };
}

DataDimension DataDimension::frequency(u_int64_t bins, std::string name)
{
    return { std::move(name), bins, 1, Role::FREQUENCY };
}

DataDimension DataDimension::spatial(u_int64_t size, char axis, u_int64_t stride)
{
    Role r = [axis]() {
        switch (axis) {
        case 'x':
            return Role::SPATIAL_X;
        case 'y':
            return Role::SPATIAL_Y;
        case 'z':
        default:
            return Role::SPATIAL_Z;
        }
    }();
    return { std::string("spatial_") + axis, size, stride, r };
}

std::vector<DataDimension> DataDimension::create_dimensions(
    DataModality modality,
    const std::vector<u_int64_t>& shape,
    MemoryLayout layout)
{
    std::vector<DataDimension> dims;
    auto strides = calculate_strides(shape, layout);

    switch (modality) {
    case DataModality::AUDIO_1D:
        if (shape.size() != 1) {
            throw std::invalid_argument("AUDIO_1D requires 1D shape");
        }
        dims.push_back(DataDimension::time(shape[0]));
        break;

    case DataModality::AUDIO_MULTICHANNEL:
        if (shape.size() != 2) {
            throw std::invalid_argument("AUDIO_MULTICHANNEL requires 2D shape [samples, channels]");
        }
        dims.push_back(DataDimension::time(shape[0]));
        dims.push_back(DataDimension::channel(shape[1], strides[1]));
        break;

    case DataModality::IMAGE_2D:
        if (shape.size() != 2) {
            throw std::invalid_argument("IMAGE_2D requires 2D shape [height, width]");
        }
        dims.push_back(DataDimension::spatial(shape[0], 'y', strides[0]));
        dims.push_back(DataDimension::spatial(shape[1], 'x', strides[1]));
        break;

    case DataModality::IMAGE_COLOR:
        if (shape.size() != 3) {
            throw std::invalid_argument("IMAGE_COLOR requires 3D shape [height, width, channels]");
        }
        dims.push_back(DataDimension::spatial(shape[0], 'y', strides[0]));
        dims.push_back(DataDimension::spatial(shape[1], 'x', strides[1]));
        dims.push_back(DataDimension::channel(shape[2], strides[2]));
        break;

    case DataModality::SPECTRAL_2D:
        if (shape.size() != 2) {
            throw std::invalid_argument("SPECTRAL_2D requires 2D shape [time_windows, frequency_bins]");
        }
        dims.push_back(DataDimension::time(shape[0], "time_windows"));
        dims.push_back(DataDimension::frequency(shape[1]));
        dims[1].stride = strides[1];
        break;

    case DataModality::VOLUMETRIC_3D:
        if (shape.size() != 3) {
            throw std::invalid_argument("VOLUMETRIC_3D requires 3D shape [x, y, z]");
        }
        dims.push_back(DataDimension::spatial(shape[0], 'x', strides[0]));
        dims.push_back(DataDimension::spatial(shape[1], 'y', strides[1]));
        dims.push_back(DataDimension::spatial(shape[2], 'z', strides[2]));
        break;

    case DataModality::VIDEO_GRAYSCALE:
        if (shape.size() != 3) {
            throw std::invalid_argument("VIDEO_GRAYSCALE requires 3D shape [frames, height, width]");
        }
        dims.push_back(DataDimension::time(shape[0], "frames"));
        dims.push_back(DataDimension::spatial(shape[1], 'y', strides[1]));
        dims.push_back(DataDimension::spatial(shape[2], 'x', strides[2]));
        break;

    case DataModality::VIDEO_COLOR:
        if (shape.size() != 4) {
            throw std::invalid_argument("VIDEO_COLOR requires 4D shape [frames, height, width, channels]");
        }
        dims.push_back(DataDimension::time(shape[0], "frames"));
        dims.push_back(DataDimension::spatial(shape[1], 'y', strides[1]));
        dims.push_back(DataDimension::spatial(shape[2], 'x', strides[2]));
        dims.push_back(DataDimension::channel(shape[3], strides[3]));
        break;

    default:
        throw std::invalid_argument("Unsupported modality for dimension creation");
    }

    return dims;
}

std::vector<u_int64_t> DataDimension::calculate_strides(
    const std::vector<u_int64_t>& shape,
    MemoryLayout layout)
{
    if (shape.empty())
        return {};

    std::vector<u_int64_t> strides(shape.size());

    if (layout == MemoryLayout::ROW_MAJOR) {
        auto reversed_shape = shape | std::views::reverse;
        std::exclusive_scan(
            reversed_shape.begin(),
            reversed_shape.end(),
            strides.rbegin(),
            1U,
            std::multiplies<u_int64_t> {});
    } else {
        std::exclusive_scan(
            shape.begin(),
            shape.end(),
            strides.begin(),
            1U,
            std::multiplies<u_int64_t> {});
    }

    return strides;
}

}
