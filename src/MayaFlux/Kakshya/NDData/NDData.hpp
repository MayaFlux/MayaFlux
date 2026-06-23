#pragma once

#include <glm/glm.hpp>

#include "typeindex"

namespace MayaFlux::Kakshya {

namespace detail {

    template <typename V>
    struct span_const_from_vector_variant;

    template <typename... Vecs>
    struct span_const_from_vector_variant<std::variant<Vecs...>> {
        using type = std::variant<std::span<const typename Vecs::value_type>...>;
    };

} // namespace detail

/**
 * @enum GpuDataFormat
 * @brief GPU data formats with explicit precision levels
 */
enum class GpuDataFormat : uint8_t {
    FLOAT32, // 32-bit float (standard GPU)
    VEC2_F32, // glm::vec2 (32-bit components)
    VEC3_F32, // glm::vec3 (32-bit components) — not a sampled image format
    VEC4_F32, // glm::vec4 (32-bit components)

    FLOAT64, // 64-bit double (audio precision)
    VEC2_F64, // glm::dvec2 (64-bit components)
    VEC3_F64, // glm::dvec3 (64-bit components)
    VEC4_F64, // glm::dvec4 (64-bit components)

    INT32,
    UINT32,

    UINT8, // uint8_t  — R8 / RGBA8 texel data
    UINT16, // uint16_t — R16F raw half-float storage, packed formats
};

/**
 * @brief Byte size of one element of a GpuDataFormat.
 *
 * Single authoritative source for format sizing. Used by TextureAccess,
 * ShaderSpec, and any other system that needs to derive byte counts from
 * GpuDataFormat without duplicating the switch.
 *
 * @param fmt GpuDataFormat value.
 * @return Byte size of one element, or 0 for unrecognised formats.
 */
MAYAFLUX_API size_t gpu_data_format_bytes(GpuDataFormat fmt) noexcept;

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
 * @brief Type traits to determine if a type is a valid DataVariant element.
 *
 * This trait is used to constrain template functions that operate on DataVariant types,
 * ensuring that only supported data types are used in the context of NDData containers.
 */
template <typename T>
struct is_data_variant_element : std::false_type { };

/**  high precision floating point data (double) (audio) */
template <>
struct is_data_variant_element<double> : std::true_type { };

/**  standard precision floating point data (float) (params) */
template <>
struct is_data_variant_element<float> : std::true_type { };

/**  8-bit unsigned integer data (uint8_t) (images, compressed audio) */
template <>
struct is_data_variant_element<uint8_t> : std::true_type { };

/**  16-bit unsigned integer data (uint16_t) (CD audio, images) */
template <>
struct is_data_variant_element<uint16_t> : std::true_type { };

/**  32-bit unsigned integer data (uint32_t) (high precision int) */
template <>
struct is_data_variant_element<uint32_t> : std::true_type { };

/**  complex data (spectral) */
template <>
struct is_data_variant_element<std::complex<float>> : std::true_type { };

/**  high precision complex data (spectral) */
template <>
struct is_data_variant_element<std::complex<double>> : std::true_type { };

/**  2D vector data (glm::vec2) */
template <>
struct is_data_variant_element<glm::vec2> : std::true_type { };

/**  3D vector data (glm::vec3) */
template <>
struct is_data_variant_element<glm::vec3> : std::true_type { };

/**  4D vector data (glm::vec4) */
template <>
struct is_data_variant_element<glm::vec4> : std::true_type { };

/**  4x4 matrix data (glm::mat4) */
template <>
struct is_data_variant_element<glm::mat4> : std::true_type { };

/**  Concept to constrain types to valid DataVariant elements */
template <typename T>
concept DataVariantElement = is_data_variant_element<std::remove_cvref_t<T>>::value;

/**
 * @brief Data modality types for cross-modal analysis
 */
enum class DataModality : uint8_t {
    AUDIO_1D, ///< 1D audio signal
    AUDIO_MULTICHANNEL, ///< Multi-channel audio
    IMAGE_2D, ///< 2D image (grayscale or single channel)
    IMAGE_COLOR, ///< 2D RGB/RGBA image
    IMAGE_COLOR_ARRAY, ///< 4D (idx + 2D + color)
    VIDEO_GRAYSCALE, ///< 3D video (time + 2D grayscale)
    VIDEO_COLOR, ///< 4D video (time + 2D + color)
    TEXTURE_2D, ///< 2D texture data
    TENSOR_ND, ///< N-dimensional tensor
    SPECTRAL_2D, ///< 2D spectral data (time + frequency)
    VOLUMETRIC_3D, ///< 3D volumetric data
    VERTICES_3D, ///< 3D vertex data (positions, normals, etc.)
    VERTEX_POSITIONS_3D, // glm::vec3 - vertex positions
    VERTEX_NORMALS_3D, // glm::vec3 - vertex normals
    VERTEX_TANGENTS_3D, // glm::vec3 - tangent vectors
    VERTEX_COLORS_RGB, // glm::vec3 - RGB colors
    VERTEX_COLORS_RGBA, // glm::vec4 - RGBA colors
    TEXTURE_COORDS_2D, // glm::vec2 - UV coordinates
    TRANSFORMATION_MATRIX, // glm::mat4 - transform matrices
    SCALAR_F32, ///< Single-channel float data
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
        MIP_LEVEL, ///< Mipmap levels
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
     * @brief Convenience constructor for an array dimension.
     * @param count Number of array elements
     * @param name Optional name (default: "array")
     * @return DataDimension representing an array
     */
    static DataDimension spatial_1d(uint64_t width);

    /**
     * @brief Convenience constructor for a 2D spatial dimension.
     * @param width Width in elements
     * @param height Height in elements
     * @return DataDimension representing 2D spatial data
     */
    static DataDimension spatial_2d(uint64_t width, uint64_t height);

    /**
     * @brief Convenience constructor for a 3D spatial dimension.
     * @param width Width in elements
     * @param height Height in elements
     * @param depth Depth in elements
     * @return DataDimension representing 3D spatial data
     */
    static DataDimension spatial_3d(uint64_t width, uint64_t height, uint64_t depth);

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
     * @brief Create dimension for mipmap levels.
     */
    static DataDimension mipmap_levels(uint64_t levels);

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

using DataSpanVariant = typename detail::span_const_from_vector_variant<DataVariant>::type;

/**
 * @class FrameView
 * @brief Zero-copy typed view over one frame of container storage.
 *
 * FrameView is the return type of NDDataContainer::get_frame(). It holds a
 * DataSpanVariant — a span into the container's live storage — without copying
 * or converting data. The active span alternative matches the container's native
 * element type: uint8_t for 8-bit image and video, uint16_t for 16-bit image,
 * float for HDR image, double for audio.
 *
 * Callers retrieve typed data via as(), which returns an empty span on type
 * mismatch rather than throwing. element_type() provides a runtime query for
 * callers whose branch depends on the native type.
 *
 * @par Lifetime
 * The span inside FrameView points directly into the owning container's internal
 * buffer. It is valid only for the duration of the current processing turn.
 * FrameView must not be stored across buffer cycles, frame callbacks, or any
 * point at which the container may write new data.
 *
 * @par Threading
 * FrameView is not thread-safe. The container's read lock (if any) is held only
 * during get_frame_span_impl(); it is released before FrameView is returned.
 * Callers must not access the view concurrently with container writes.
 *
 * @see NDDataContainer::get_frame(), DataSpanVariant, DataVariantElement
 */
class FrameView {
public:
    /** @brief Construct an empty FrameView. empty() returns true. */
    FrameView() = default;

    /**
     * @brief Construct from a DataSpanVariant produced by get_frame_span_impl().
     * @param span Active alternative must match the container's native element type.
     */
    explicit FrameView(DataSpanVariant span)
        : m_span(span)
    {
    }

    /** @brief Returns true if the view holds no elements. */
    [[nodiscard]] bool empty() const
    {
        return std::visit([](const auto& s) { return s.empty(); }, m_span);
    }

    /** @brief Number of elements in the frame, in units of the native element type. */
    [[nodiscard]] size_t size() const
    {
        return std::visit([](const auto& s) { return s.size(); }, m_span);
    }

    /**
     * @brief Runtime query for the native element type of this frame.
     *
     * Returns std::type_index of the active span's value_type. Typical values:
     * - typeid(uint8_t)  — 8-bit image/video (RGBA, etc.)
     * - typeid(uint16_t) — 16-bit image (UNORM or half-float encoded)
     * - typeid(float)    — HDR image (R32F/RGBA32F)
     * - typeid(double)   — audio
     *
     * Use this to branch once before calling as<T>() when the container type
     * is not statically known at the call site.
     */
    [[nodiscard]] std::type_index element_type() const
    {
        return std::visit([](const auto& s) {
            return std::type_index(typeid(typename std::decay_t<decltype(s)>::value_type));
        },
            m_span);
    }

    /**
     * @brief Return a typed span over the frame data without conversion.
     *
     * Returns an empty span if T does not match the native element type.
     * Callers should query element_type() first when the type is not statically known.
     * The returned span is valid only for the lifetime of the owning container's frame buffer.
     *
     * @tparam T Element type. Must satisfy DataVariantElement.
     * @return Span of const T, empty on type mismatch.
     */
    /**
     * @brief Return a typed span over the frame data without conversion or allocation.
     *
     * Returns an empty span if T does not match the native element type.
     * No exception is thrown on mismatch; check empty() or call element_type() first
     * when the container type is not statically known.
     *
     * The returned span aliases the container's internal buffer directly.
     * See class-level lifetime and threading notes.
     *
     * @tparam T Element type. Must satisfy DataVariantElement.
     * @return Span of const T into live storage, empty on type mismatch.
     */
    template <DataVariantElement T>
    [[nodiscard]] std::span<const T> as() const
    {
        const auto* s = std::get_if<std::span<const T>>(&m_span);
        return s ? *s : std::span<const T> {};
    }

    /**
     * @brief Direct access to the underlying DataSpanVariant for internal use.
     *
     * Intended for infrastructure (processors, Yantra pipeline) that must
     * forward the view without branching on type. Not for general callers.
     */
    [[nodiscard]] const DataSpanVariant& raw() const { return m_span; }

private:
    DataSpanVariant m_span;
};

} // namespace MayaFlux::Kakshya
