#pragma once

namespace MayaFlux::Kakshya {

/**
 * @brief Memory layout for multi-dimensional data.
 *
 * Specifies how multi-dimensional data is mapped to linear memory.
 * - ROW_MAJOR: Last dimension varies fastest (C/C++ style).
 * - COLUMN_MAJOR: First dimension varies fastest (Fortran/MATLAB style).
 *
 * This abstraction enables flexible, efficient access patterns for
 * digital-first, data-driven workflows, unconstrained by analog conventions.
 */
enum class MemoryLayout : u_int8_t {
    ROW_MAJOR, ///< C/C++ style (last dimension varies fastest)
    COLUMN_MAJOR ///< Fortran/MATLAB style (first dimension varies fastest)
};

/**
 * @brief Data organization strategy for multi-channel/multi-frame data.
 *
 * Determines how logical units (channels, frames) are stored in memory.
 */
enum class OrganizationStrategy : u_int8_t {
    INTERLEAVED, ///< Single DataVariant with interleaved data (LRLRLR for stereo)
    PLANAR, ///< Separate DataVariant per logical unit (LLL...RRR for stereo)
    HYBRID, ///< Mixed approach based on access patterns
    USER_DEFINED ///< Custom organization
};

/**
 * @brief Multi-type data storage for different precision needs.
 *
 * DataVariant enables containers to store and expose data in the most
 * appropriate format for the application, supporting high-precision,
 * standard-precision, integer, and complex types. This abstraction
 * is essential for digital-first, data-driven processing pipelines.
 */
using DataVariant = std::variant<
    std::vector<double>, ///< High precision floating point
    std::vector<float>, ///< Standard precision floating point
    std::vector<u_int8_t>, ///< 8-bit data (images, compressed audio)
    std::vector<u_int16_t>, ///< 16-bit data (CD audio, images)
    std::vector<u_int32_t>, ///< 32-bit data (high precision int)
    std::vector<std::complex<float>>, ///< Complex data (spectral)
    std::vector<std::complex<double>> ///< High precision complex
    >;

/**
 * @brief Data modality types for cross-modal analysis
 */
enum class DataModality : u_int8_t {
    AUDIO_1D, // 1D audio signal
    AUDIO_MULTICHANNEL, // Multi-channel audio
    IMAGE_2D, // 2D image (grayscale or single channel)
    IMAGE_COLOR, // 2D RGB/RGBA image
    VIDEO_GRAYSCALE, // 3D video (time + 2D grayscale)
    VIDEO_COLOR, // 4D video (time + 2D + color)
    TEXTURE_2D, // 2D texture data
    TENSOR_ND, // N-dimensional tensor
    SPECTRAL_2D, // 2D spectral data (time + frequency)
    VOLUMETRIC_3D, // 3D volumetric data
    UNKNOWN
};

/**
 * @brief Minimal dimension descriptor focusing on structure only.
 *
 * DataDimension describes a single axis of an N-dimensional dataset,
 * providing semantic hints (such as TIME, CHANNEL, SPATIAL_X, etc.)
 * and structural information (name, size, stride).
 *
 * This abstraction enables containers to describe arbitrary data
 * organizations, supporting digital-first, data-driven processing
 * without imposing analog metaphors (e.g., "track", "tape", etc.).
 */
struct DataDimension {
    /**
     * @brief Semantic role of the dimension.
     *
     * Used to indicate the intended interpretation of the dimension,
     * enabling generic algorithms to adapt to data structure.
     */
    enum class Role : u_int8_t {
        TIME, ///< Temporal progression (samples, frames, steps)
        CHANNEL, ///< Parallel streams (audio channels, color channels)
        SPATIAL_X, ///< Spatial X axis (images, tensors)
        SPATIAL_Y, ///< Spatial Y axis
        SPATIAL_Z, ///< Spatial Z axis
        FREQUENCY, ///< Spectral/frequency axis
        CUSTOM ///< User-defined or application-specific
    };

    std::string name; ///< Human-readable identifier for the dimension
    u_int64_t size {}; ///< Number of elements in this dimension
    u_int64_t stride {}; ///< Memory stride (elements between consecutive indices)
    Role role = Role::CUSTOM; ///< Semantic hint for common operations

    DataDimension() = default;

    /**
     * @brief Construct a dimension descriptor.
     * @param n Name of the dimension
     * @param s Size (number of elements)
     * @param st Stride (default: 1)
     * @param r Semantic role (default: CUSTOM)
     */
    DataDimension(std::string n, u_int64_t s, u_int64_t st = 1, Role r = Role::CUSTOM);

    /**
     * @brief Convenience constructor for a temporal (time) dimension.
     * @param samples Number of samples/frames
     * @param name Optional name (default: "time")
     * @return DataDimension representing time
     */
    static DataDimension time(u_int64_t samples, std::string name = "time");

    /**
     * @brief Convenience constructor for a channel dimension.
     * @param count Number of channels
     * @param stride Memory stride (default: 1)
     * @return DataDimension representing channels
     */
    static DataDimension channel(u_int64_t count, u_int64_t stride = 1);

    /**
     * @brief Convenience constructor for a frequency dimension.
     * @param bins Number of frequency bins
     * @param name Optional name (default: "frequency")
     * @return DataDimension representing frequency
     */
    static DataDimension frequency(u_int64_t bins, std::string name = "frequency");

    /**
     * @brief Convenience constructor for a spatial dimension.
     * @param size Number of elements along this axis
     * @param axis Axis character ('x', 'y', or 'z')
     * @param stride Memory stride (default: 1)
     * @return DataDimension representing a spatial axis
     */
    static DataDimension spatial(u_int64_t size, char axis, u_int64_t stride = 1);

    using DataModule = std::pair<std::vector<DataVariant>, std::vector<DataDimension>>;

    template <typename T>
    DataModule create_for_modality(
        DataModality modality,
        const std::vector<u_int64_t>& shape,
        T default_value = T {},
        MemoryLayout layout = MemoryLayout::ROW_MAJOR)
    {
        auto dims = create_dimensions(modality, shape, layout);
        auto variants = create_variants(modality, shape, default_value);

        return { std::move(variants), std::move(dims) };
    }

    static std::vector<DataDimension> create_dimensions(
        DataModality modality,
        const std::vector<u_int64_t>& shape,
        MemoryLayout layout = MemoryLayout::ROW_MAJOR);

    template <typename T>
    DataModule create_audio_1d(u_int64_t samples, T default_value = T {})
    {
        return create_for_modality(DataModality::AUDIO_1D, { samples }, default_value);
    }

    template <typename T>
    DataModule create_audio_multichannel(u_int64_t samples, u_int64_t channels, T default_value = T {})
    {
        return create_for_modality(DataModality::AUDIO_MULTICHANNEL, { samples, channels }, default_value);
    }

    template <typename T>
    DataModule create_image_2d(u_int64_t height, u_int64_t width, T default_value = T {})
    {
        return create_for_modality(DataModality::IMAGE_2D, { height, width }, default_value);
    }

    template <typename T>
    DataModule create_spectral_2d(u_int64_t time_windows, u_int64_t frequency_bins, T default_value = T {})
    {
        return create_for_modality(DataModality::SPECTRAL_2D, { time_windows, frequency_bins }, default_value);
    }

    /**
     * @brief Calculate strides based on shape and memory layout.
     */
    static std::vector<uint64_t> calculate_strides(
        const std::vector<uint64_t>& shape,
        MemoryLayout layout);

private:
    template <typename T>
    static std::vector<DataVariant> create_variants(
        DataModality modality,
        const std::vector<u_int64_t>& shape,
        T default_value,
        OrganizationStrategy org = OrganizationStrategy::PLANAR)
    {
        std::vector<DataVariant> variants;

        if (org == OrganizationStrategy::INTERLEAVED) {
            u_int64_t total = std::accumulate(shape.begin(), shape.end(), u_int64_t(1), std::multiplies<>());
            variants.emplace_back(std::vector<T>(total, default_value));
            return variants;
        }

        switch (modality) {
        case DataModality::AUDIO_1D:
            variants.emplace_back(std::vector<T>(shape[0], default_value));
            break;

        case DataModality::AUDIO_MULTICHANNEL: {
            u_int64_t samples = shape[0];
            u_int64_t channels = shape[1];
            variants.reserve(channels);
            for (u_int64_t ch = 0; ch < channels; ++ch) {
                variants.emplace_back(std::vector<T>(samples, default_value));
            }
            break;
        }

        case DataModality::IMAGE_2D:
            variants.emplace_back(std::vector<T>(shape[0] * shape[1], default_value));
            break;

        case DataModality::IMAGE_COLOR: {
            u_int64_t height = shape[0];
            u_int64_t width = shape[1];
            u_int64_t channels = shape[2];
            u_int64_t pixels = height * width;
            variants.reserve(channels);
            for (u_int64_t ch = 0; ch < channels; ++ch) {
                variants.emplace_back(std::vector<T>(pixels, default_value));
            }
            break;
        }

        case DataModality::SPECTRAL_2D:
            variants.emplace_back(std::vector<T>(shape[0] * shape[1], default_value));
            break;

        case DataModality::VOLUMETRIC_3D:
            variants.emplace_back(std::vector<T>(shape[0] * shape[1] * shape[2], default_value));
            break;

        case DataModality::VIDEO_GRAYSCALE: {
            u_int64_t frames = shape[0];
            u_int64_t height = shape[1];
            u_int64_t width = shape[2];
            u_int64_t frame_size = height * width;
            variants.reserve(frames);
            for (u_int64_t f = 0; f < frames; ++f) {
                variants.emplace_back(std::vector<T>(frame_size, default_value));
            }
            break;
        }

        case DataModality::VIDEO_COLOR: {
            u_int64_t frames = shape[0];
            u_int64_t height = shape[1];
            u_int64_t width = shape[2];
            u_int64_t channels = shape[3];
            u_int64_t frame_size = height * width;
            variants.reserve(frames * channels);
            for (u_int64_t f = 0; f < frames; ++f) {
                for (u_int64_t ch = 0; ch < channels; ++ch) {
                    variants.emplace_back(std::vector<T>(frame_size, default_value));
                }
            }
            break;
        }

        default:
            u_int64_t total = std::accumulate(shape.begin(), shape.end(), u_int64_t(1), std::multiplies<>());
            variants.emplace_back(std::vector<T>(total, default_value));
            break;
        }

        return variants;
    }
};
}
