#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"

#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class WindowAccessProcessor
 * @brief Default DataProcessor for WindowContainer.
 *
 * Reads pixel data from the last completed swapchain frame into the
 * container's processed_data. The DataVariant type is selected at
 * on_attach() time based on the live swapchain format reported by
 * DisplayService::get_swapchain_format, ensuring HDR and packed formats
 * are stored with their native precision rather than truncated to uint8.
 *
 * Format → DataVariant element type:
 *   8-bit UNORM/SRGB  → std::vector<uint8_t>
 *   16-bit SFLOAT     → std::vector<uint16_t>  (raw half-float bits)
 *   10-bit packed     → std::vector<uint32_t>  (packed word per pixel)
 *   32-bit SFLOAT     → std::vector<float>
 *
 * One readback per frame regardless of region count — region extraction
 * is a CPU-side crop performed by WindowContainer::get_region_data().
 */
class MAYAFLUX_API WindowAccessProcessor : public DataProcessor {
public:
    WindowAccessProcessor() = default;
    ~WindowAccessProcessor() override = default;

    /**
     * @brief Attach to a WindowContainer.
     *        Queries the live swapchain format and caches surface traits.
     *        Throws std::invalid_argument if container is not a WindowContainer.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Release all cached state.
     */
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Execute a full-surface pixel readback.
     *        Allocates a format-appropriate DataVariant and updates
     *        container ProcessingState: PROCESSING → PROCESSED.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Whether a process() call is currently executing.
     */
    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(); }

    /**
     * @brief Byte size of the last successful readback.
     */
    [[nodiscard]] size_t get_last_readback_bytes() const { return m_last_readback_bytes; }

    /**
     * @brief The surface format currently in use for readback allocation.
     */
    [[nodiscard]] Core::GraphicsSurfaceInfo::SurfaceFormat get_surface_format() const
    {
        return m_surface_format;
    }

private:
    std::atomic<bool> m_is_processing { false };

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    size_t m_last_readback_bytes { 0 };

    Core::GraphicsSurfaceInfo::SurfaceFormat m_surface_format {
        Core::GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_SRGB
    };
};

} // namespace MayaFlux::Kakshya
