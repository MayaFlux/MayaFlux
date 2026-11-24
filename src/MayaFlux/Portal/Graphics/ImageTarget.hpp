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

    /** @brief A render target that compute passes can also write. */
    bool storage {};

    /** @brief A render target of six square faces that samples as a cube. */
    bool cube {};

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
 * @struct TargetArea
 * @brief Where in an image target one destination draws.
 *
 * The layer is the array layer or cube face drawn into. The area is the part of
 * that layer the destination's view fills, in the NDC convention of
 * Kinesis::Scissor, so it stays right when the target is resized. Unset is the
 * whole layer. The viewport maps clip space onto the area and the draw is
 * clipped to it, together with the processor's own scissor, which is resolved
 * against the area.
 */
struct TargetArea {
    uint32_t layer {};
    std::optional<Kinesis::Scissor> area;

    [[nodiscard]] bool operator==(const TargetArea& other) const noexcept
    {
        return layer == other.layer && area.has_value() == other.area.has_value()
            && (!area || *area == *other.area);
    }
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
 * The images rest in shader read layout, so they can be bound as textures by
 * any buffer, including one drawn into a window or another target. A target
 * can have several color images drawn by one pass, layers or cube faces that
 * destinations draw into one at a time, color images that compute passes also
 * write, and a depth image that stays sampleable after the pass.
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

        /**
         * @brief Formats of further color images drawn by the same pass, as outputs 1, 2 and so on.
         *
         * Each is cleared to zero. They are drawn with blending off and need
         * the device's independent blend. Refused when a format cannot be drawn
         * into and sampled, or the device cannot blend independently.
         */
        std::vector<ImageFormat> extra_formats;

        /** @brief Layers of every color image. One is a plain 2D image. Not combined with mip levels. */
        uint32_t layers { 1 };

        /** @brief Six square faces that sample as a cube. Takes the place of layers. Not combined with mip levels. */
        bool cube { false };

        /** @brief Let compute passes write the color images as storage images. The format has to allow it. */
        bool storage { false };

        /** @brief Keep the depth image after each pass, in shader read layout, so it can be sampled. Needs depth. */
        bool sample_depth { false };
    };

    ~ImageTarget();

    ImageTarget(const ImageTarget&) = delete;
    ImageTarget& operator=(const ImageTarget&) = delete;
    ImageTarget(ImageTarget&&) = delete;
    ImageTarget& operator=(ImageTarget&&) = delete;

    /** @brief The first color image. Bind it as a texture to show what was drawn. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& image() const noexcept { return m_image; }

    /** @brief Color image @p index: 0 is image(), the others follow Config::extra_formats. Null past the last. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& image(size_t index) const noexcept
    {
        static const std::shared_ptr<Core::VKImage> none;
        if (index == 0)
            return m_image;
        return index - 1 < m_extra.size() ? m_extra[index - 1] : none;
    }

    /** @brief How many color images the pass draws. */
    [[nodiscard]] size_t attachment_count() const noexcept { return 1 + m_extra.size(); }

    /** @brief The depth image, or null when the target was made without one. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& depth_image() const noexcept { return m_depth; }

    /** @brief Layers of every color image. A cube has six. */
    [[nodiscard]] uint32_t layers() const noexcept { return m_layers; }

    [[nodiscard]] bool cube() const noexcept { return m_cube; }

    /** @brief Whether compute passes can write the color images. */
    [[nodiscard]] bool storage() const noexcept { return m_storage; }

    /** @brief Whether the depth image stays sampleable after each pass. */
    [[nodiscard]] bool sample_depth() const noexcept { return m_sample_depth; }

    [[nodiscard]] uint32_t width() const noexcept { return m_width.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t height() const noexcept { return m_height.load(std::memory_order_relaxed); }

    /**
     * @brief Give both images another size.
     * @return False when a side is zero or an allocation failed. After a failure
     *         the target is as it was.
     *
     * The images are new objects and the old ones are released. A buffer that
     * bound the target through RenderProcessor::bind_texture follows on its
     * next execute. Any other holder of the color image compares generation()
     * with the value it last saw and binds image() again. The next scope
     * clears. Calls are ordered with drawing by the caller.
     */
    bool resize(uint32_t width, uint32_t height);

    /** @brief Counts the resizes that gave the target new images. */
    [[nodiscard]] uint64_t generation() const noexcept { return m_generation.load(std::memory_order_acquire); }

    [[nodiscard]] FilterMode mip_filter() const noexcept { return m_mip_filter; }

    /** @brief Color format of the first image, as a pipeline drawing into it declares it. */
    [[nodiscard]] vk::Format color_format() const noexcept { return m_color_format; }

    /** @brief Color formats of every image the pass draws, in output order. */
    [[nodiscard]] std::vector<vk::Format> color_formats() const
    {
        std::vector<vk::Format> formats { m_color_format };
        formats.insert(formats.end(), m_extra_formats.begin(), m_extra_formats.end());
        return formats;
    }

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
        std::vector<std::shared_ptr<Core::VKImage>> extra,
        std::shared_ptr<Core::VKImage> depth,
        const Config& config);

    std::shared_ptr<Core::VKImage> m_image;
    std::vector<std::shared_ptr<Core::VKImage>> m_extra;
    std::shared_ptr<Core::VKImage> m_depth;
    std::atomic<uint32_t> m_width {};
    std::atomic<uint32_t> m_height {};
    std::atomic<uint64_t> m_generation { 0 };
    FilterMode m_mip_filter { FilterMode::LINEAR };
    bool m_full_chain {};
    uint32_t m_layers { 1 };
    bool m_cube {};
    bool m_storage {};
    bool m_sample_depth {};
    vk::Format m_color_format {};
    std::vector<vk::Format> m_extra_formats;
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
