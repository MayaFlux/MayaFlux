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

std::vector<DataDimension> DataDimension::create_dimensions_for_modality(DataModality modality, const std::vector<u_int64_t>& shape)
{
    switch (modality) {
    case DataModality::AUDIO_1D:
        if (shape.size() != 1) {
            throw std::invalid_argument("AUDIO_1D requires 1D shape");
        }
        return { DataDimension::time(shape[0]) };

    case DataModality::AUDIO_MULTICHANNEL:
        if (shape.size() != 2) {
            throw std::invalid_argument("AUDIO_MULTICHANNEL requires 2D shape [samples, channels]");
        }
        return {
            DataDimension::time(shape[0]),
            DataDimension::channel(shape[1])
        };

    case DataModality::IMAGE_2D:
        if (shape.size() != 2) {
            throw std::invalid_argument("IMAGE_2D requires 2D shape [height, width]");
        }
        return {
            DataDimension::spatial(shape[0], 'y'), // height
            DataDimension::spatial(shape[1], 'x') // width
        };

    case DataModality::IMAGE_COLOR:
        if (shape.size() != 3) {
            throw std::invalid_argument("IMAGE_COLOR requires 3D shape [height, width, channels]");
        }
        return {
            DataDimension::spatial(shape[0], 'y'),
            DataDimension::spatial(shape[1], 'x'),
            DataDimension::channel(shape[2])
        };

    case DataModality::SPECTRAL_2D:
        if (shape.size() != 2) {
            throw std::invalid_argument("SPECTRAL_2D requires 2D shape [time_windows, frequency_bins]");
        }
        return {
            DataDimension::time(shape[0], "time_windows"),
            DataDimension::frequency(shape[1])
        };

    case DataModality::VOLUMETRIC_3D:
        if (shape.size() != 3) {
            throw std::invalid_argument("VOLUMETRIC_3D requires 3D shape [x, y, z]");
        }
        return {
            DataDimension::spatial(shape[0], 'x'),
            DataDimension::spatial(shape[1], 'y'),
            DataDimension::spatial(shape[2], 'z')
        };

    case DataModality::VIDEO_GRAYSCALE:
        if (shape.size() != 3) {
            throw std::invalid_argument("VIDEO_GRAYSCALE requires 3D shape [frames, height, width]");
        }
        return {
            DataDimension::time(shape[0], "frames"),
            DataDimension::spatial(shape[1], 'y'),
            DataDimension::spatial(shape[2], 'x')
        };

    case DataModality::VIDEO_COLOR:
        if (shape.size() != 4) {
            throw std::invalid_argument("VIDEO_COLOR requires 4D shape [frames, height, width, channels]");
        }
        return {
            DataDimension::time(shape[0], "frames"),
            DataDimension::spatial(shape[1], 'y'),
            DataDimension::spatial(shape[2], 'x'),
            DataDimension::channel(shape[3])
        };

    default:
        throw std::invalid_argument("Unsupported modality for dimension creation");
    }
}

}
