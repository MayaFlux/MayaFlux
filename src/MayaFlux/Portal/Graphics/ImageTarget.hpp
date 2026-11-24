#pragma once

#include <vulkan/vulkan.hpp>

#include "GraphicsUtils.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @class ImageTarget
 * @brief An image that draws land in instead of a window.
 *
 * Shared by pointer: every buffer aimed at it holds the same one, and the root
 * draws them together in one scope, as it does for one window. The clear color,
 * the optional depth image and when the image redraws belong to the target, so
 * buffers drawing into it cannot disagree.
 *
 * The image rests in shader read layout, so it can be bound as a texture by any
 * buffer, including one drawn into a window or another target.
 */
class MAYAFLUX_API ImageTarget {
public:
    /** @brief Size, format and policy of a new target. */
    struct Config {
        uint32_t width {};
        uint32_t height {};
        ImageFormat format { ImageFormat::RGBA8 };
        bool depth { false };
        std::array<float, 4> clear_color { 0.0F, 0.0F, 0.0F, 0.0F };
        bool continuous { false };
    };

    ~ImageTarget();

    ImageTarget(const ImageTarget&) = delete;
    ImageTarget& operator=(const ImageTarget&) = delete;
    ImageTarget(ImageTarget&&) = delete;
    ImageTarget& operator=(ImageTarget&&) = delete;

    /** @brief The color image. Bind it as a texture to show what was drawn. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& image() const noexcept { return m_image; }

    /** @brief The depth image, or null when the target was made without one. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& depth_image() const noexcept { return m_depth; }

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

    /** @brief Color format of the image, as a pipeline drawing into it declares it. */
    [[nodiscard]] vk::Format color_format() const noexcept { return m_color_format; }

    /** @brief Depth format, or undefined when there is no depth image. */
    [[nodiscard]] vk::Format depth_format() const noexcept { return m_depth_format; }

    [[nodiscard]] std::array<float, 4> clear_color() const noexcept;

    /** @brief Set the color every scope clears to. Takes effect at the next redraw. */
    void set_clear_color(std::array<float, 4> color) noexcept;

    /** @brief Redraw every cycle instead of only when something changed or was requested. */
    void set_continuous(bool continuous) noexcept { m_continuous.store(continuous, std::memory_order_relaxed); }

    [[nodiscard]] bool continuous() const noexcept { return m_continuous.load(std::memory_order_relaxed); }

    /** @brief Draw at the next opportunity. Safe from any thread. */
    void request_redraw() noexcept { m_redraw.store(true, std::memory_order_release); }

    /** @brief Whether this target draws at all. An inactive target keeps its last image. */
    void set_active(bool active) noexcept { m_active.store(active, std::memory_order_relaxed); }

    [[nodiscard]] bool active() const noexcept { return m_active.load(std::memory_order_relaxed); }

    /**
     * @brief Whether this cycle draws, clearing the request.
     *
     * Always true for a continuous target. Called by the root once per cycle.
     */
    [[nodiscard]] bool take_redraw() noexcept
    {
        const bool requested = m_redraw.exchange(false, std::memory_order_acq_rel);
        return m_continuous.load(std::memory_order_relaxed) || requested;
    }

private:
    friend class TextureLoom;

    ImageTarget(
        std::shared_ptr<Core::VKImage> image,
        std::shared_ptr<Core::VKImage> depth,
        const Config& config);

    std::shared_ptr<Core::VKImage> m_image;
    std::shared_ptr<Core::VKImage> m_depth;
    uint32_t m_width {};
    uint32_t m_height {};
    vk::Format m_color_format {};
    vk::Format m_depth_format { vk::Format::eUndefined };

    std::array<std::atomic<float>, 4> m_clear;
    std::atomic<bool> m_continuous { false };
    std::atomic<bool> m_redraw { true };
    std::atomic<bool> m_active { true };
};

}
