#pragma once

#include <glm/glm.hpp>

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
enum class MemoryLayout : uint8_t {
    ROW_MAJOR, ///< C/C++ style (last dimension varies fastest)
    COLUMN_MAJOR ///< Fortran/MATLAB style (first dimension varies fastest)
};

/**
 * @brief Data organization strategy for multi-channel/multi-frame data.
 *
 * Determines how logical units (channels, frames) are stored in memory.
 */
enum class OrganizationStrategy : uint8_t {
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
    std::vector<uint8_t>, ///< 8-bit data (images, compressed audio)
    std::vector<uint16_t>, ///< 16-bit data (CD audio, images)
    std::vector<uint32_t>, ///< 32-bit data (high precision int)
    std::vector<std::complex<float>>, ///< Complex data (spectral)
    std::vector<std::complex<double>>, ///< High precision complex
    std::vector<glm::vec2>, ///< 2D vector data
    std::vector<glm::vec3>, ///< 3D vector data
    std::vector<glm::vec4>, ///< 4D vector data
    std::vector<glm::mat4> ///< 4x4 matrix data
    >;

/**
 * @brief Data modality types for cross-modal analysis
 */
enum class DataModality : uint8_t {
    AUDIO_1D, ///< 1D audio signal
    AUDIO_MULTICHANNEL, ///< Multi-channel audio
    IMAGE_2D, ///< 2D image (grayscale or single channel)
    IMAGE_COLOR, ///< 2D RGB/RGBA image
    VIDEO_GRAYSCALE, ///< 3D video (time + 2D grayscale)
    VIDEO_COLOR, ///< 4D video (time + 2D + color)
    TEXTURE_2D, ///< 2D texture data
    TENSOR_ND, ///< N-dimensional tensor
    SPECTRAL_2D, ///< 2D spectral data (time + frequency)
    VOLUMETRIC_3D, ///< 3D volumetric data
    VERTEX_POSITIONS_3D, // glm::vec3 - vertex positions
    VERTEX_NORMALS_3D, // glm::vec3 - vertex normals
    VERTEX_TANGENTS_3D, // glm::vec3 - tangent vectors
    VERTEX_COLORS_RGB, // glm::vec3 - RGB colors
    VERTEX_COLORS_RGBA, // glm::vec4 - RGBA colors
    TEXTURE_COORDS_2D, // glm::vec2 - UV coordinates
    TRANSFORMATION_MATRIX, // glm::mat4 - transform matrices
    UNKNOWN ///< Unknown or undefined modality
};

/**
 * @brief Convert DataModality enum to string representation.
 * @param modality DataModality value
 * @return String view of the modality name
 */
std::string_view modality_to_string(DataModality modality);

/**
 * @brief Check if a modality represents structured data (vectors, matrices).
 * @param modality DataModality value
 * @return True if structured, false otherwise
 */
inline bool is_structured_modality(DataModality modality)
{
    switch (modality) {
    case DataModality::VERTEX_POSITIONS_3D:
    case DataModality::VERTEX_NORMALS_3D:
    case DataModality::VERTEX_TANGENTS_3D:
    case DataModality::VERTEX_COLORS_RGB:
    case DataModality::VERTEX_COLORS_RGBA:
    case DataModality::TEXTURE_COORDS_2D:
    case DataModality::TRANSFORMATION_MATRIX:
        return true;
    default:
        return false;
    }
}

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
struct MAYAFLUX_API DataDimension {
    /**
     * @brief Semantic role of the dimension.
     *
     * Used to indicate the intended interpretation of the dimension,
     * enabling generic algorithms to adapt to data structure.
     */
    enum class Role : uint8_t {
        TIME, ///< Temporal progression (samples, frames, steps)
        CHANNEL, ///< Parallel streams (audio channels, color channels)
        SPATIAL_X, ///< Spatial X axis (images, tensors)
        SPATIAL_Y, ///< Spatial Y axis
        SPATIAL_Z, ///< Spatial Z axis
        FREQUENCY, ///< Spectral/frequency axis
        POSITION, ///< Vertex positions (3D space)
        NORMAL, ///< Surface normals
        TANGENT, ///< Tangent vectors
        BITANGENT, ///< Bitangent vectors
        UV, ///< Texture coordinates
        COLOR, ///< Color data (RGB/RGBA)
        INDEX, ///< Index buffer data
        CUSTOM ///< User-defined or application-specific
    };

    /**
     * @brief Grouping information for sub-dimensions.
     *
     * Used to indicate that this dimension is composed of groups
     * of sub-dimensions (e.g., color channels grouped per pixel).
     */
    struct ComponentGroup {
        uint8_t count;
        uint8_t offset;

        ComponentGroup()
            : count(0)
            , offset(0)
        {
        }
        ComponentGroup(uint8_t c, uint8_t o = 0)
            : count(c)
            , offset(o)
        {
        }
    };

    std::optional<ComponentGroup> grouping;

    std::string name; ///< Human-readable identifier for the dimension
    uint64_t size {}; ///< Number of elements in this dimension
    uint64_t stride {}; ///< Memory stride (elements between consecutive indices)
    Role role = Role::CUSTOM; ///< Semantic hint for common operations

    DataDimension() = default;

    /**
     * @brief Construct a dimension descriptor.
     * @param n Name of the dimension
     * @param s Size (number of elements)
     * @param st Stride (default: 1)
     * @param r Semantic role (default: CUSTOM)
     */
    DataDimension(std::string n, uint64_t s, uint64_t st = 1, Role r = Role::CUSTOM);

    /**
     * @brief Convenience constructor for a temporal (time) dimension.
     * @param samples Number of samples/frames
     * @param name Optional name (default: "time")
     * @return DataDimension representing time
     */
    static DataDimension time(uint64_t samples, std::string name = "time");

    /**
     * @brief Convenience constructor for a channel dimension.
     * @param count Number of channels
     * @param stride Memory stride (default: 1)
     * @return DataDimension representing channels
     */
    static DataDimension channel(uint64_t count, uint64_t stride = 1);

    /**
     * @brief Convenience constructor for a frequency dimension.
     * @param bins Number of frequency bins
     * @param name Optional name (default: "frequency")
     * @return DataDimension representing frequency
     */
    static DataDimension frequency(uint64_t bins, std::string name = "frequency");

    /**
     * @brief Convenience constructor for a spatial dimension.
     * @param size Number of elements along this axis
     * @param axis Axis character ('x', 'y', or 'z')
     * @param stride Memory stride (default: 1)
     * @param name Optional name (default: "pixels")
     * @return DataDimension representing a spatial axis
     */
    static DataDimension spatial(uint64_t size, char axis, uint64_t stride = 1, std::string name = "spatial");

    /**
     * @brief Create dimension with component grouping
     * @param name Dimension name
     * @param element_count Number of elements (not components)
     * @param components_per_element Components per element (e.g., 3 for vec3)
     * @param role Semantic role
     */
    static DataDimension grouped(std::string name, uint64_t element_count, uint8_t components_per_element, Role role = Role::CUSTOM);

    /**
     * @brief Create dimension for vertex positions (vec3)
     */
    static DataDimension vertex_positions(uint64_t count);

    /**
     * @brief Create dimension for vertex normals (vec3)
     */
    static DataDimension vertex_normals(uint64_t count);

    /**
     * @brief Create dimension for texture coordinates (vec2)
     */
    static DataDimension texture_coords(uint64_t count);

    /**
     * @brief Create dimension for colors (vec3 or vec4)
     */
    static DataDimension vertex_colors(uint64_t count, bool has_alpha = false);

    /**
     * @brief Data container combining variants and dimensions.
     */
    using DataModule = std::pair<std::vector<DataVariant>, std::vector<DataDimension>>;

    /**
     * @brief Create data module for a specific modality.
     * @tparam T Data type for storage
     * @param modality Target data modality
     * @param shape Dimensional sizes
     * @param default_value Initial value for elements
     * @param layout Memory layout strategy
     * @param strategy Organization strategy
     * @return DataModule with appropriate structure
     */
    template <typename T>
    static DataModule create_for_modality(
        DataModality modality,
        const std::vector<uint64_t>& shape,
        T default_value = T {},
        MemoryLayout layout = MemoryLayout::ROW_MAJOR,
        OrganizationStrategy strategy = OrganizationStrategy::PLANAR)
    {
        auto dims = create_dimensions(modality, shape, layout);
        auto variants = create_variants(modality, shape, default_value, strategy);

        return { std::move(variants), std::move(dims) };
    }

    /**
     * @brief Create dimension descriptors for a data modality.
     * @param modality Target data modality
     * @param shape Dimensional sizes
     * @param layout Memory layout strategy
     * @return Vector of DataDimension objects
     */
    static std::vector<DataDimension> create_dimensions(
        DataModality modality,
        const std::vector<uint64_t>& shape,
        MemoryLayout layout = MemoryLayout::ROW_MAJOR);

    /**
     * @brief Create 1D audio data module.
     * @tparam T Data type for storage
     * @param samples Number of audio samples
     * @param default_value Initial value for elements
     * @return DataModule for 1D audio
     */
    template <typename T>
    static DataModule create_audio_1d(uint64_t samples, T default_value = T {})
    {
        return create_for_modality(DataModality::AUDIO_1D, { samples }, default_value);
    }

    /**
     * @brief Create multi-channel audio data module.
     * @tparam T Data type for storage
     * @param samples Number of audio samples
     * @param channels Number of audio channels
     * @param default_value Initial value for elements
     * @return DataModule for multi-channel audio
     */
    template <typename T>
    static DataModule create_audio_multichannel(uint64_t samples, uint64_t channels, T default_value = T {})
    {
        return create_for_modality(DataModality::AUDIO_MULTICHANNEL, { samples, channels }, default_value);
    }

    /**
     * @brief Create 2D image data module.
     * @tparam T Data type for storage
     * @param height Image height in pixels
     * @param width Image width in pixels
     * @param default_value Initial value for elements
     * @return DataModule for 2D image
     */
    template <typename T>
    static DataModule create_image_2d(uint64_t height, uint64_t width, T default_value = T {})
    {
        return create_for_modality(DataModality::IMAGE_2D, { height, width }, default_value);
    }

    /**
     * @brief Create 2D spectral data module.
     * @tparam T Data type for storage
     * @param time_windows Number of time windows
     * @param frequency_bins Number of frequency bins
     * @param default_value Initial value for elements
     * @return DataModule for spectral data
     */
    template <typename T>
    static DataModule create_spectral_2d(uint64_t time_windows, uint64_t frequency_bins, T default_value = T {})
    {
        return create_for_modality(DataModality::SPECTRAL_2D, { time_windows, frequency_bins }, default_value);
    }

    /**
     * @brief Calculate memory strides based on shape and layout.
     * @param shape Dimensional sizes
     * @param layout Memory layout strategy
     * @return Vector of stride values for each dimension
     */
    static std::vector<uint64_t> calculate_strides(
        const std::vector<uint64_t>& shape,
        MemoryLayout layout);

private:
    /**
     * @brief Create data variants for a specific modality.
     * @tparam T Data type for storage
     * @param modality Target data modality
     * @param shape Dimensional sizes
     * @param default_value Initial value for elements
     * @param org Organization strategy
     * @return Vector of DataVariant objects
     */
    template <typename T>
    static std::vector<DataVariant> create_variants(
        DataModality modality,
        const std::vector<uint64_t>& shape,
        T default_value,
        OrganizationStrategy org = OrganizationStrategy::PLANAR)
    {
        std::vector<DataVariant> variants;

        if (org == OrganizationStrategy::INTERLEAVED) {
            uint64_t total = std::accumulate(shape.begin(), shape.end(), uint64_t(1), std::multiplies<>());
            variants.emplace_back(std::vector<T>(total, default_value));
            return variants;
        }

        switch (modality) {
        case DataModality::AUDIO_1D:
            variants.emplace_back(std::vector<T>(shape[0], default_value));
            break;

        case DataModality::AUDIO_MULTICHANNEL: {
            uint64_t samples = shape[0];
            uint64_t channels = shape[1];
            variants.reserve(channels);
            for (uint64_t ch = 0; ch < channels; ++ch) {
                variants.emplace_back(std::vector<T>(samples, default_value));
            }
            break;
        }

        case DataModality::IMAGE_2D:
            variants.emplace_back(std::vector<T>(shape[0] * shape[1], default_value));
            break;

        case DataModality::IMAGE_COLOR: {
            uint64_t height = shape[0];
            uint64_t width = shape[1];
            uint64_t channels = shape[2];
            uint64_t pixels = height * width;
            variants.reserve(channels);
            for (uint64_t ch = 0; ch < channels; ++ch) {
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
            uint64_t frames = shape[0];
            uint64_t height = shape[1];
            uint64_t width = shape[2];
            uint64_t frame_size = height * width;
            variants.reserve(frames);
            for (uint64_t f = 0; f < frames; ++f) {
                variants.emplace_back(std::vector<T>(frame_size, default_value));
            }
            break;
        }

        case DataModality::VIDEO_COLOR: {
            uint64_t frames = shape[0];
            uint64_t height = shape[1];
            uint64_t width = shape[2];
            uint64_t channels = shape[3];
            uint64_t frame_size = height * width;
            variants.reserve(frames * channels);
            for (uint64_t f = 0; f < frames; ++f) {
                for (uint64_t ch = 0; ch < channels; ++ch) {
                    variants.emplace_back(std::vector<T>(frame_size, default_value));
                }
            }
            break;
        }

        default:
            uint64_t total = std::accumulate(shape.begin(), shape.end(), uint64_t(1), std::multiplies<>());
            variants.emplace_back(std::vector<T>(total, default_value));
            break;
        }

        return variants;
    }
};

} // namespace MayaFlux::Kakshya
