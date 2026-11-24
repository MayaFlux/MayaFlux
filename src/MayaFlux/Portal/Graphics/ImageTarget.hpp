#pragma once

#include <vulkan/vulkan.hpp>

#include "GraphicsUtils.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Portal::Graphics {

/** @brief Immutable properties that determine whether an image can be reused. */
struct ImageKey {
    enum class Kind : uint8_t {
        SAMPLED_2D,
        SAMPLED_ARRAY,
        STORAGE_2D,
        RENDER_TARGET,
        DEPTH
    };

    uint32_t width {};
    uint32_t height {};
    uint32_t layers { 1 };
    uint32_t mip_levels { 1 };
    ImageFormat format {};
    Kind kind { Kind::SAMPLED_2D };

    [[nodiscard]] bool operator==(const ImageKey&) const = default;
};

/** @brief Hash image allocation properties for keyed reuse. */
struct MAYAFLUX_API ImageKeyHash {
    [[nodiscard]] size_t operator()(const ImageKey& key) const noexcept;
};

/** @brief Image retained by the producing owner. */
struct ImageCacheEntry {
    ImageKey key;
    std::shared_ptr<Core::VKImage> image;
};

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

        /** @brief Keep what was drawn before instead of clearing at every redraw. */
        bool preserve { false };

        /** @brief Levels of the color image, 0 for a full chain. Levels below the first are regenerated after every draw. */
        uint32_t mip_levels { 1 };
        FilterMode mip_filter { FilterMode::LINEAR };
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

    [[nodiscard]] uint32_t width() const noexcept { return m_width.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t height() const noexcept { return m_height.load(std::memory_order_relaxed); }

    /**
     * @brief Give both images another size.
     * @return False when a side is zero or an allocation failed. After a failure
     *         the target is as it was.
     *
     * The images are new objects and the old ones are released, so whoever
     * bound the color image binds image() again. The next scope clears. Calls
     * are ordered with drawing by the caller.
     */
    bool resize(uint32_t width, uint32_t height);

    [[nodiscard]] FilterMode mip_filter() const noexcept { return m_mip_filter; }

    /** @brief Color format of the image, as a pipeline drawing into it declares it. */
    [[nodiscard]] vk::Format color_format() const noexcept { return m_color_format; }

    /** @brief Depth format, or undefined when there is no depth image. */
    [[nodiscard]] vk::Format depth_format() const noexcept { return m_depth_format; }

    [[nodiscard]] std::array<float, 4> clear_color() const noexcept;

    /** @brief Keep what was drawn before, or clear at every redraw. */
    void set_preserve(bool preserve) noexcept { m_preserve.store(preserve, std::memory_order_relaxed); }

    [[nodiscard]] bool preserve() const noexcept { return m_preserve.load(std::memory_order_relaxed); }

    /** @brief Clear at the next redraw even when the target preserves, and draw at the next opportunity. */
    void request_clear() noexcept { m_redraw.fetch_or(k_draw | k_clear, std::memory_order_release); }

    /**
     * @brief Whether the scope being recorded clears, consuming a pending request.
     *
     * Always true for a target that does not preserve. Called by the render flow once per scope.
     */
    [[nodiscard]] bool take_clear() noexcept
    {
        const bool pending = (m_redraw.fetch_and(static_cast<uint8_t>(~k_clear), std::memory_order_acq_rel) & k_clear) != 0;
        return pending || !m_preserve.load(std::memory_order_relaxed);
    }

    /** @brief Set the color every clear uses. Asks for a clear and a redraw. */
    void set_clear_color(std::array<float, 4> color) noexcept;

    /** @brief Redraw every cycle instead of only when something changed or was requested. */
    void set_continuous(bool continuous) noexcept { m_continuous.store(continuous, std::memory_order_relaxed); }

    [[nodiscard]] bool continuous() const noexcept { return m_continuous.load(std::memory_order_relaxed); }

    /** @brief Draw at the next opportunity. Safe from any thread. */
    void request_redraw() noexcept { m_redraw.fetch_or(k_draw, std::memory_order_release); }

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
        const bool requested = (m_redraw.fetch_and(static_cast<uint8_t>(~k_draw), std::memory_order_acq_rel) & k_draw) != 0;
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
    std::atomic<uint32_t> m_width {};
    std::atomic<uint32_t> m_height {};
    FilterMode m_mip_filter { FilterMode::LINEAR };
    bool m_full_chain {};
    vk::Format m_color_format {};
    vk::Format m_depth_format { vk::Format::eUndefined };

    std::array<std::atomic<float>, 4> m_clear;
    std::atomic<bool> m_continuous { false };
    std::atomic<bool> m_preserve { false };
    static constexpr uint8_t k_draw = 1;
    static constexpr uint8_t k_clear = 2;
    std::atomic<uint8_t> m_redraw { k_draw | k_clear };
    std::atomic<bool> m_active { true };
};

}
