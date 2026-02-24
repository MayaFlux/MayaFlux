#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/Region/Region.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class WindowAccessProcessor
 * @brief Default DataProcessor for WindowContainer.
 *
 * Reads pixel data from the last completed swapchain frame into the
 * container's processed_data as a single interleaved std::vector<uint8_t>
 * with modality IMAGE_COLOR.
 *
 * Guarantee: each process() call populates get_processed_data() with data
 * from the last fully-presented frame — the swapchain image whose fence has
 * already signaled. Never reads from the image currently being written to.
 *
 * Performs a single full-surface GPU readback per process() call into
 * processed_data[0] as an interleaved std::vector<uint8_t> (IMAGE_COLOR).
 * Region selection is a CPU-side concern handled by WindowContainer::get_region_data().
 *
 * One readback per frame regardless of how many regions are loaded — region
 * count has no effect on GPU work.
 */
class MAYAFLUX_API WindowAccessProcessor : public DataProcessor {
public:
    WindowAccessProcessor() = default;
    ~WindowAccessProcessor() override = default;

    /**
     * @brief Attach to a WindowContainer.
     *        Caches surface dimensions from the container's structure.
     *        Throws if container is not a WindowContainer.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Release cached state.
     */
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Execute pixel readback for the active region.
     *        Reads from the last completed swapchain frame via VisualContainerUtils.
     *        Updates container ProcessingState: PROCESSING → PROCESSED.
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

private:
    std::atomic<bool> m_is_processing { false };

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    uint32_t m_channels { 0 };
    size_t m_last_readback_bytes { 0 };

    /**
     * @brief Resolve a Region to a clamped pixel rect.
     * @return {x_offset, y_offset, pixel_width, pixel_height}.
     *         Returns the full surface rect if the region has fewer than
     *         2 coordinates or produces a degenerate rect.
     */
    // [[nodiscard]] std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>
    // resolve_pixel_rect(const Region& region) const;
};

} // namespace MayaFlux::Kakshya
