#pragma once

namespace MayaFlux::Kakshya {

/**
 * @enum ImageFormat
 * @brief User-friendly image format enum
 *
 * Abstracts Vulkan formats for API convenience. Portal maps it to
 * vk::Format; this layer carries no Vulkan types.
 */
enum class ImageFormat : uint8_t {
    // Normalized formats
    R8, ///< Single channel 8-bit
    RG8, ///< Two channel 8-bit
    RGB8, ///< Three channel 8-bit
    RGBA8, ///< Four channel 8-bit
    RGBA8_SRGB, ///< Four channel 8-bit sRGB

    BGRA8, ///< 8-bit BGRA unsigned normalized
    BGRA8_SRGB, ///< 8-bit BGRA sRGB

    // Floating point formats
    R16F, ///< Single channel 16-bit float
    RG16F, ///< Two channel 16-bit float
    RGBA16F, ///< Four channel 16-bit float
    R32F, ///< Single channel 32-bit float
    RG32F, ///< Two channel 32-bit float
    RGBA32F, ///< Four channel 32-bit float

    R16, ///< Single channel 16-bit unsigned integer
    RG16, ///< Two channel 16-bit unsigned integer
    RGBA16, ///< Four channel 16-bit unsigned integer

    // Depth/stencil formats
    DEPTH16, ///< 16-bit depth
    DEPTH24, ///< 24-bit depth
    DEPTH32F, ///< 32-bit float depth
    DEPTH24_STENCIL8 ///< 24-bit depth + 8-bit stencil
};

/**
 * @struct ImageData
 * @brief Raw image data loaded from file or read back from a GPU image.
 *
 * Pixel storage is a variant over uint8, uint16, and float, chosen by the
 * loader based on the source format. 8-bit formats (PNG, JPG, BMP, TGA)
 * populate the uint8 variant. 16-bit PNG populates the uint16 variant.
 * Floating-point formats (EXR, HDR) populate the float variant.
 *
 * The declared ImageFormat must match the active variant. Accessors below
 * enforce this; direct member access is permitted but callers are
 * responsible for consistency.
 */
struct ImageData {
    using PixelStorage = std::variant<
        std::vector<uint8_t>,
        std::vector<uint16_t>,
        std::vector<float>>;

    PixelStorage pixels;
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint32_t channels { 0 };
    ImageFormat format { ImageFormat::RGBA8 };

    /**
     * @brief Total byte size of pixel storage, dispatched on variant.
     */
    [[nodiscard]] size_t byte_size() const
    {
        return std::visit(
            [](const auto& vec) { return vec.size() * sizeof(typename std::decay_t<decltype(vec)>::value_type); },
            pixels);
    }

    /**
     * @brief Raw data pointer, dispatched on variant. For upload paths.
     */
    [[nodiscard]] const void* data() const
    {
        return std::visit(
            [](const auto& vec) -> const void* { return vec.data(); },
            pixels);
    }

    /**
     * @brief Writable raw data pointer, dispatched on variant. For download paths.
     */
    [[nodiscard]] void* data()
    {
        return std::visit(
            [](auto& vec) -> void* { return vec.data(); },
            pixels);
    }

    /**
     * @brief Allocate zeroed pixel storage of the variant @p format calls for.
     *
     * The variant matches what is_consistent() expects: uint8 for the 8-bit
     * formats, uint16 for the 16-bit and half-float formats, float for the
     * 32-bit float formats. Holds width * height * channels elements.
     *
     * @return The sized ImageData, or nullopt when any dimension or the
     *         channel count is zero, or the format has no storage mapping.
     */
    [[nodiscard]] static std::optional<ImageData> allocate(
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        ImageFormat format);

    /**
     * @brief Number of pixel elements (not bytes), dispatched on variant.
     */
    [[nodiscard]] size_t element_count() const
    {
        return std::visit(
            [](const auto& vec) { return vec.size(); },
            pixels);
    }

    /**
     * @brief Check that the active variant matches the declared format.
     *
     * Callers producing ImageData should invoke this to validate before
     * handing the data to downstream consumers.
     */
    [[nodiscard]] bool is_consistent() const;

    /**
     * @brief Typed accessors. Return nullptr if variant does not match.
     */
    [[nodiscard]] const std::vector<uint8_t>* as_uint8() const { return std::get_if<std::vector<uint8_t>>(&pixels); }
    [[nodiscard]] const std::vector<uint16_t>* as_uint16() const { return std::get_if<std::vector<uint16_t>>(&pixels); }
    [[nodiscard]] const std::vector<float>* as_float() const { return std::get_if<std::vector<float>>(&pixels); }

    [[nodiscard]] std::vector<uint8_t>* as_uint8() { return std::get_if<std::vector<uint8_t>>(&pixels); }
    [[nodiscard]] std::vector<uint16_t>* as_uint16() { return std::get_if<std::vector<uint16_t>>(&pixels); }
    [[nodiscard]] std::vector<float>* as_float() { return std::get_if<std::vector<float>>(&pixels); }
};

} // namespace MayaFlux::Kakshya
