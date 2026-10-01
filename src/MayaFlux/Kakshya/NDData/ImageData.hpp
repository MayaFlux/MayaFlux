#pragma once

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Kakshya {

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
    Portal::Graphics::ImageFormat format { Portal::Graphics::ImageFormat::RGBA8 };

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
