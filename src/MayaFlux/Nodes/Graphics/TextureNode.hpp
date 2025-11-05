#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes {

/**
 * @class TextureNode
 * @brief Base class for texture-generating nodes
 *
 * Provides common functionality for managing texture dimensions and pixel data
 * in RGBA float format. Derived classes implement compute_frame() to define
 * specific texture generation or processing algorithms.
 *
 * Texture data is stored as a flat array in row-major order:
 * [R0,G0,B0,A0, R1,G1,B1,A1, ..., Rn,Gn,Bn,An]
 */
class MAYAFLUX_API TextureNode : public Node {
protected:
    uint32_t m_width;
    uint32_t m_height;
    std::vector<float> m_pixel_buffer; // RGBA format

public:
    TextureNode(uint32_t width, uint32_t height);

    /**
     * @brief Computes the texture frame
     *
     * Pure virtual method implemented by derived classes to generate or
     * update texture data in m_pixel_buffer.
     */
    virtual void compute_frame() = 0;

    double process_sample(double /*input*/) override
    {
        compute_frame();
        return 0.;
    }

    /**
     * @brief Get pixel buffer
     * @return Span of pixel data in RGBA format
     */
    [[nodiscard]] std::span<const float> get_pixel_buffer() const { return m_pixel_buffer; }

    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }

    /**
     * @brief Get total number of pixels
     * @return width * height
     */
    [[nodiscard]] size_t get_pixel_count() const
    {
        return static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    }

    /**
     * @brief Get buffer size in bytes
     * @return Total size of pixel buffer
     */
    [[nodiscard]] size_t get_buffer_size() const
    {
        return m_pixel_buffer.size() * sizeof(float);
    }

protected:
    /**
     * @brief Set pixel color at (x, y)
     * @param x X coordinate
     * @param y Y coordinate
     * @param r Red channel [0, 1]
     * @param g Green channel [0, 1]
     * @param b Blue channel [0, 1]
     * @param a Alpha channel [0, 1] (default: 1.0)
     */
    void set_pixel(uint32_t x, uint32_t y, float r, float g, float b, float a = 1.0F);

    /**
     * @brief Get pixel color at (x, y)
     * @param x X coordinate
     * @param y Y coordinate
     * @return RGBA values as array [r, g, b, a]
     */
    [[nodiscard]] std::array<float, 4> get_pixel(uint32_t x, uint32_t y) const;

    /**
     * @brief Fill entire texture with solid color
     * @param r Red channel
     * @param g Green channel
     * @param b Blue channel
     * @param a Alpha channel (default: 1.0)
     */
    void fill(float r, float g, float b, float a = 1.0F);

    /**
     * @brief Clear texture to black
     */
    void clear();
};

} // namespace MayaFlux::Nodes
