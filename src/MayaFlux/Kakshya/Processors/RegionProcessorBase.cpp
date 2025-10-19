#include "RegionProcessorBase.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

void RegionProcessorBase::on_attach(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container) {
        throw std::invalid_argument("Container cannot be null");
    }

    m_container_weak = container;
    m_structure = container->get_structure();

    m_cache_manager = std::make_unique<RegionCacheManager>(m_max_cache_size);
    m_cache_manager->initialize();

    m_current_position.resize(m_structure.get_frame_size());

    organize_container_data(container);

    container->mark_ready_for_processing(true);
}

void RegionProcessorBase::on_detach(std::shared_ptr<SignalSourceContainer> /*container*/)
{
    m_container_weak.reset();
    m_cache_manager.reset();
    m_organized_regions.clear();
    m_current_position.clear();
}

void RegionProcessorBase::cache_region_if_needed(const RegionSegment& segment, const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!m_auto_caching || !m_cache_manager)
        return;

    if (m_cache_manager->get_cached_segment(segment)) {
        return;
    }

    uint64_t segment_size = segment.get_total_elements();
    if (segment_size <= m_max_cache_size / 10) { // Use max 10% of cache per segment
        try {
            RegionCache cache;
            cache.data = container->get_region_data(segment.source_region);
            cache.source_region = segment.source_region;
            cache.load_time = std::chrono::steady_clock::now();

            m_cache_manager->cache_region(cache);
        } catch (const std::exception& e) {
            // Silently fail caching - not critical
        }
    }
}

bool RegionProcessorBase::advance_position(std::vector<uint64_t>& position, uint64_t steps, const OrganizedRegion* region)
{
    if (position.empty())
        return false;

    for (auto& pos : position) {
        pos += steps;
    }

    if ((region != nullptr) && region->looping_enabled && !region->loop_start.empty() && !region->loop_end.empty()) {
        for (size_t dim = 0; dim < std::min(position.size(), region->loop_start.size()); ++dim) {
            if (position[dim] >= region->loop_end[dim]) {
                position[dim] = region->loop_start[dim] + (position[dim] - region->loop_end[dim]);
            }
        }
    }

    auto frame_count = m_structure.get_frame_count();
    for (auto& pos : position) {
        if (pos >= frame_count) {
            pos = frame_count - 1;
            return false;
        }
    }

    return true;
}

void RegionProcessorBase::ensure_output_dimensioning(std::vector<DataVariant>& output_data, const std::vector<uint64_t>& required_shape)
{
    if (output_data.size() < required_shape[1]) {
        output_data.resize(required_shape[1], DataVariant {});
    }

    for (auto& data : output_data) {
        std::visit([required_shape](auto& dt) {
            if (dt.size() < required_shape[0]) {
                dt.resize(required_shape[0]);
            }
        },
            data);
    }
}

}
