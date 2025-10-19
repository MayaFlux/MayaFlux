#include "test_config.h"

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {
class MockSignalSourceContainer : public SignalSourceContainer {
public:
    MockSignalSourceContainer()
        : SignalSourceContainer()
    {
        m_processed_data.resize(1);
        m_processed_data[0] = std::vector<double>(1024, 0.0);
        setup_structure();
    }

    void setup_structure()
    {
        DataModality modality = (m_num_channels > 1)
            ? DataModality::AUDIO_MULTICHANNEL
            : DataModality::AUDIO_1D;

        std::vector<uint64_t> shape;
        if (modality == DataModality::AUDIO_1D) {
            shape = { m_num_frames };
        } else {
            shape = { m_num_frames, m_num_channels };
        }

        OrganizationStrategy org = OrganizationStrategy::PLANAR;
        MemoryLayout layout = MemoryLayout::ROW_MAJOR;

        m_data_structure = ContainerDataStructure(modality, org, layout);
        m_data_structure.dimensions = DataDimension::create_dimensions(modality, shape, layout);

        m_data_structure.time_dims = m_num_frames;
        m_data_structure.channel_dims = m_num_channels;
    }

    void add_test_region_group(const std::string& name)
    {
        RegionGroup group(name);
        m_region_groups[name] = group;
    }

    void add_test_region_to_group(const std::string& group_name, const Region& region)
    {
        if (m_region_groups.find(group_name) != m_region_groups.end()) {
            m_region_groups[group_name].add_region(region);
        }
    }

    void set_test_data(const std::vector<double>& data)
    {
        if (m_data_structure.organization == OrganizationStrategy::INTERLEAVED) {
            m_processed_data.resize(1);
            m_processed_data[0] = data;
        } else {
            m_processed_data.resize(m_num_channels);
            for (uint32_t ch = 0; ch < m_num_channels; ++ch) {
                std::vector<double> channel_data;
                channel_data.reserve(data.size() / m_num_channels);
                for (size_t i = ch; i < data.size(); i += m_num_channels) {
                    channel_data.push_back(data[i]);
                }
                m_processed_data[ch] = std::move(channel_data);
            }
        }
    }

    void set_multi_channel_test_data(const std::vector<std::vector<double>>& channel_data)
    {
        m_processed_data.clear();
        m_processed_data.reserve(channel_data.size());

        for (const auto& channel : channel_data) {
            m_processed_data.emplace_back(Kakshya::DataVariant { channel });
        }

        m_num_channels = static_cast<uint32_t>(channel_data.size());
        m_data_structure.organization = OrganizationStrategy::PLANAR;
    }

    std::vector<DataDimension> get_dimensions() const override
    {
        return m_data_structure.dimensions;
    }

    uint64_t get_total_elements() const override
    {
        return m_data_structure.get_total_elements();
    }

    MemoryLayout get_memory_layout() const override
    {
        return m_data_structure.memory_layout;
    }

    void set_memory_layout(MemoryLayout layout) override
    {
        m_data_structure.memory_layout = layout;
        setup_structure(); // Recreate dimensions with new layout
    }

    // Use cached values for performance (like SoundStreamContainer)
    uint64_t get_num_frames() const override
    {
        return m_num_frames;
    }

    uint64_t get_frame_size() const override
    {
        return m_num_channels;
    }

    std::vector<DataVariant> get_region_data(const Region&) const override
    {
        if (m_data_structure.organization == OrganizationStrategy::INTERLEAVED) {
            return { m_processed_data[0] };
        }

        std::vector<DataVariant> result;

        result.reserve(m_processed_data.size());

        for (const auto& channel_data : m_processed_data) {
            result.push_back(channel_data);
        }
        return result;
    }

    std::vector<DataVariant> get_region_group_data(const RegionGroup& regions) const override
    {
        /* std::vector<DataVariant> all_data;
        for (const auto& region : regions) {
            auto region_data = get_region_data(region);
            all_data.insert(all_data.end(), region_data.begin(), region_data.end());
        }
        return all_data; */
        return {};
    }

    std::vector<DataVariant> get_segments_data(const std::vector<RegionSegment>&) const override
    {
        return {};
    }

    void set_region_data(const Region&, const std::vector<DataVariant>& data) override
    {
        if (!data.empty()) {
            m_processed_data = data;
        }
    }

    std::span<const double> get_frame(uint64_t) const override
    {
        static std::vector<double> empty;
        return {};
    }

    void get_frames(std::span<double>, uint64_t, uint64_t) const override
    {
        // No-op for mock
    }

    double get_value_at(const std::vector<uint64_t>&) const override
    {
        return 0.0;
    }

    void set_value_at(const std::vector<uint64_t>&, double) override
    {
        // No-op for mock
    }

    void add_region_group(const RegionGroup& group) override
    {
        m_region_groups[group.name] = group;
    }

    const RegionGroup& get_region_group(const std::string& name) const override
    {
        static RegionGroup empty("empty");
        auto it = m_region_groups.find(name);
        if (it != m_region_groups.end()) {
            return it->second;
        }
        return empty;
    }

    std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override
    {
        return m_region_groups;
    }

    void remove_region_group(const std::string& name) override
    {
        m_region_groups.erase(name);
    }

    bool is_region_loaded(const Region&) const override
    {
        return true;
    }

    void load_region(const Region&) override
    {
        // No-op for mock
    }

    void unload_region(const Region&) override
    {
        // No-op for mock
    }

    uint64_t coordinates_to_linear_index(const std::vector<uint64_t>&) const override
    {
        return 0;
    }

    std::vector<uint64_t> linear_index_to_coordinates(uint64_t) const override
    {
        return { 0, 0 };
    }

    void clear() override
    {
        m_processed_data.clear();
    }

    void lock() override
    {
        // No-op for mock
    }

    void unlock() override
    {
        // No-op for mock
    }

    bool try_lock() override
    {
        return true;
    }

    const void* get_raw_data() const override
    {
        return m_processed_data.data();
    }

    bool has_data() const override
    {
        return !m_processed_data.empty();
    }

    void register_state_change_callback(
        std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> callback) override
    {
        m_state_change_callback = callback;
    }

    void unregister_state_change_callback() override
    {
        m_state_change_callback = nullptr;
    }

    bool is_ready_for_processing() const override
    {
        return m_ready_for_processing;
    }

    void mark_ready_for_processing(bool ready) override
    {
        m_ready_for_processing = ready;
    }

    void create_default_processor() override
    {
        // No-op for mock
    }

    void process_default() override
    {
        m_processing_state = ProcessingState::PROCESSED;
    }

    void set_default_processor(std::shared_ptr<DataProcessor> processor) override
    {
        m_default_processor = processor;
    }

    std::shared_ptr<DataProcessor> get_default_processor() const override
    {
        return m_default_processor;
    }

    std::shared_ptr<DataProcessingChain> get_processing_chain() override
    {
        return m_processing_chain;
    }

    void set_processing_chain(std::shared_ptr<DataProcessingChain> chain) override
    {
        m_processing_chain = chain;
    }

    uint32_t register_dimension_reader(uint32_t) override
    {
        // No-op for mock
        return 0;
    }

    void unregister_dimension_reader(uint32_t) override
    {
        // No-op for mock
    }

    bool has_active_readers() const override
    {
        return false;
    }

    void mark_dimension_consumed(uint32_t, uint32_t) override
    {
        // No-op for mock
    }

    bool all_dimensions_consumed() const override
    {
        return true;
    }

    void mark_buffers_for_processing(bool) override
    {
        // No-op for mock
    }

    void mark_buffers_for_removal() override
    {
        // No-op for mock
    }

    ProcessingState get_processing_state() const override
    {
        return m_processing_state;
    }

    void set_processing_state(ProcessingState state)
    {
        m_processing_state = state;
        if (m_state_change_callback) {
            m_state_change_callback(nullptr, state);
        }
    }

    void update_processing_state(ProcessingState new_state) override
    {
        m_processing_state = new_state;
    }

    std::vector<DataVariant>& get_processed_data() override
    {
        return m_processed_variant;
    }

    const std::vector<DataVariant>& get_processed_data() const override
    {
        // m_processed_variant[0] = DataVariant(m_processed_data);
        return m_processed_variant;
    }

    inline const ContainerDataStructure& get_structure() const override { return m_data_structure; }

    inline ContainerDataStructure& get_structure() override { return m_data_structure; }

    void set_structure(ContainerDataStructure structure) override
    {
        m_data_structure = structure;
        m_ready_for_processing = false;
    }

    const std::vector<DataVariant>& get_data() override { return m_processed_data; }

private:
    uint32_t m_num_channels { 1 };
    uint64_t m_num_frames { 1024 };

    std::vector<DataVariant> m_processed_data;
    std::vector<DataVariant> m_processed_variant;

    ContainerDataStructure m_data_structure;

    std::unordered_map<std::string, RegionGroup> m_region_groups;
    ProcessingState m_processing_state { ProcessingState::IDLE };
    bool m_ready_for_processing {};

    std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> m_state_change_callback;
    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;
};
}
