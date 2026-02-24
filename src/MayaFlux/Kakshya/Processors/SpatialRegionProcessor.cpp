#include "SpatialRegionProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Kakshya {

void SpatialRegionProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "SpatialRegionProcessor: container must not be null");
    }

    const auto& dims = container->get_structure().dimensions;
    const bool has_spatial = std::ranges::any_of(dims, [](const DataDimension& d) {
        return d.role == DataDimension::Role::SPATIAL_X
            || d.role == DataDimension::Role::SPATIAL_Y
            || d.role == DataDimension::Role::SPATIAL_Z;
    });

    if (!has_spatial) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "SpatialRegionProcessor: container must have at least one spatial dimension");
    }

    m_auto_caching = false;

    RegionProcessorBase::on_attach(container);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "SpatialRegionProcessor attached ({} region(s) across {} group(s))",
        m_organized_regions.size(),
        container->get_all_region_groups().size());
}

void SpatialRegionProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& container)
{
    RegionProcessorBase::on_detach(container);
}

// =========================================================================
// organize_container_data
// =========================================================================

void SpatialRegionProcessor::organize_container_data(
    const std::shared_ptr<SignalSourceContainer>& container)
{
    m_organized_regions.clear();

    const auto groups = container->get_all_region_groups();

    for (const auto& [group_name, group] : groups) {
        for (size_t i = 0; i < group.regions.size(); ++i) {
            OrganizedRegion org(group_name, static_cast<uint32_t>(i));

            std::ranges::copy(group.attributes,
                std::inserter(org.attributes, org.attributes.end()));
            std::ranges::copy(group.regions[i].attributes,
                std::inserter(org.attributes, org.attributes.end()));

            org.attributes["group_name"] = group_name;
            org.attributes["region_index"] = i;

            org.segments.emplace_back(group.regions[i]);
            org.state = RegionState::READY;

            m_organized_regions.push_back(std::move(org));
        }
    }
}

// =========================================================================
// process
// =========================================================================

void SpatialRegionProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SpatialRegionProcessor::process — null container");
        return;
    }

    auto& processed = container->get_processed_data();

    if (processed.empty()) {
        MF_RT_TRACE(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SpatialRegionProcessor: processed_data empty, no readback available");
        container->update_processing_state(ProcessingState::IDLE);
        return;
    }

    const auto* full_surface = std::get_if<std::vector<uint8_t>>(&processed[0]);
    if (!full_surface || full_surface->empty()) {
        MF_RT_TRACE(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SpatialRegionProcessor: processed_data[0] empty, skipping extraction");
        container->update_processing_state(ProcessingState::IDLE);
        return;
    }

    if (m_organized_regions.empty()) {
        container->update_processing_state(ProcessingState::IDLE);
        return;
    }

    m_is_processing.store(true, std::memory_order_release);
    container->update_processing_state(ProcessingState::PROCESSING);

    const auto& dims = container->get_structure().dimensions;
    const std::span<const uint8_t> src { full_surface->data(), full_surface->size() };

    std::vector<DataVariant> extracts;
    extracts.reserve(m_organized_regions.size());

    for (auto& org : m_organized_regions) {
        if (org.segments.empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "SpatialRegionProcessor: OrganizedRegion '{}[{}]' has no segments, skipping",
                org.group_name, org.region_index);
            continue;
        }

        org.state = RegionState::ACTIVE;

        try {
            extracts.emplace_back(
                extract_nd_region<uint8_t>(src, org.segments[0].source_region, dims));
            org.state = RegionState::READY;
        } catch (const std::exception& e) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "SpatialRegionProcessor: extraction failed for '{}[{}]' — {}",
                org.group_name, org.region_index, e.what());
            org.state = RegionState::IDLE;
        }
    }

    processed = std::move(extracts);

    m_is_processing.store(false, std::memory_order_release);
    container->update_processing_state(ProcessingState::PROCESSED);
}

// =========================================================================
// refresh
// =========================================================================

void SpatialRegionProcessor::refresh()
{
    if (auto container = m_container_weak.lock()) {
        organize_container_data(container);

        MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "SpatialRegionProcessor: refreshed ({} region(s))",
            m_organized_regions.size());
    }
}

} // namespace MayaFlux::Kakshya
