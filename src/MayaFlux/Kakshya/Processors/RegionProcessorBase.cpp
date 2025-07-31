#include "RegionProcessorBase.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

void RegionProcessorBase::on_attach(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container) {
        throw std::invalid_argument("Container cannot be null");
    }

    m_container_weak = container;

    m_dimensions = container->get_dimensions();
    m_memory_layout = container->get_memory_layout();

    m_cache_manager = std::make_unique<RegionCacheManager>(m_max_cache_size);
    m_cache_manager->initialize();

    m_current_position.resize(m_dimensions.size(), 0);

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

    u_int64_t segment_size = segment.get_total_elements();
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

bool RegionProcessorBase::advance_position(std::vector<u_int64_t>& position, u_int64_t steps, const OrganizedRegion* region)
{
    if (position.empty() || m_dimensions.empty())
        return false;

    position[0] += steps;

    if ((region != nullptr) && region->looping_enabled && !region->loop_start.empty() && !region->loop_end.empty()) {
        for (size_t dim = 0; dim < std::min(position.size(), region->loop_start.size()); ++dim) {
            if (position[dim] >= region->loop_end[dim]) {
                position[dim] = region->loop_start[dim] + (position[dim] - region->loop_end[dim]);
            }
        }
    }

    for (size_t dim = 0; dim < position.size() - 1; ++dim) {
        if (position[dim] >= m_dimensions[dim].size) {
            position[dim] = 0;
            position[dim + 1]++;
        } else {
            break;
        }
    }

    return position.back() < m_dimensions.back().size;
}

void RegionProcessorBase::ensure_output_dimensioning(DataVariant& output_data, const std::vector<u_int64_t>& required_shape)
{
    u_int64_t required_elements = 1;
    for (auto dim : required_shape) {
        required_elements *= dim;
    }

    std::visit([required_elements](auto& data) {
        if (data.size() < required_elements) {
            data.resize(required_elements);
        }
    },
        output_data);
}

}
