#pragma once

#include "RegionProcessorBase.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SpatialRegionProcessor
 * @brief Parallel spatial extraction processor for image-modality containers.
 *
 * Operates on any SignalSourceContainer whose structure carries at least one
 * spatial dimension (SPATIAL_Y, SPATIAL_X, or SPATIAL_Z).  Unlike
 * RegionOrganizationProcessor — which advances a sequential temporal cursor
 * through audio regions — this processor extracts *all* active regions on
 * every process() call, reflecting the parallel readback semantics of spatial
 * data.
 *
 * Region source:
 *   Regions are read from the container's RegionGroup map
 *   (get_all_region_groups()).  Each Region inside each group becomes one
 *   OrganizedRegion in m_organized_regions, inheriting both group-level and
 *   region-level attributes.  organize_container_data() re-syncs this list
 *   from the live group map, so callers can mutate groups between frames by
 *   calling refresh().
 *
 * Processing contract (process()):
 *   - Expects processed_data[0] to hold the full-surface readback as a
 *     std::vector<uint8_t> placed there by the default processor
 *     (e.g. WindowAccessProcessor) before the chain runs.
 *   - Replaces processed_data with one DataVariant per active region,
 *     in group-iteration order, then region-insertion order within each group.
 *   - Each DataVariant carries the attributes from its OrganizedRegion so
 *     downstream consumers can identify the source ("group_name", "region_index").
 *   - If processed_data[0] is absent or empty, state is set to IDLE and the
 *     method returns without modifying processed_data.
 *
 * Caching:
 *   Auto-caching is disabled by default.  Spatial data from a live surface
 *   changes every frame, so caching individual regions yields no benefit
 *   under normal operation.  It can be re-enabled via set_auto_caching(true)
 *   for static or infrequently-updated surfaces (e.g. a paused framebuffer).
 *
 * Container neutrality:
 *   No WindowContainer-specific code.  Any container that satisfies the
 *   NDDimensionalContainer interface and populates processed_data[0] with a
 *   flat spatial buffer is compatible.
 */
class MAYAFLUX_API SpatialRegionProcessor : public RegionProcessorBase {
public:
    SpatialRegionProcessor() = default;
    ~SpatialRegionProcessor() override = default;

    /**
     * @brief Attach to a spatial container.
     *        Validates that the container has at least one spatial dimension.
     *        Disables auto-caching (appropriate for live surfaces).
     *        Delegates to RegionProcessorBase::on_attach which calls
     *        organize_container_data().
     * @throws std::invalid_argument if no spatial dimension is found.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Detach; delegates to RegionProcessorBase::on_detach.
     */
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Extract all active regions from processed_data[0] in parallel.
     *
     *        For each OrganizedRegion in m_organized_regions, extracts pixel
     *        data via extract_nd_region and appends to processed_data.
     *        Sets OrganizedRegion::state to ACTIVE during extraction and
     *        PROCESSED on completion.  Failed extractions are logged and
     *        skipped without aborting the frame.
     *
     * @param container Source container; processed_data[0] must hold the
     *                  full-surface buffer.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Re-sync m_organized_regions from the container's current group map.
     *        Call after mutating region groups between frames.
     */
    void refresh();

protected:
    /**
     * @brief Build m_organized_regions from get_all_region_groups().
     *        Each Region in each RegionGroup becomes one OrganizedRegion whose
     *        attributes include "group_name" (std::string) and "region_index"
     *        (size_t) for downstream identification.
     */
    void organize_container_data(
        const std::shared_ptr<SignalSourceContainer>& container) override;
};

} // namespace MayaFlux::Kakshya
