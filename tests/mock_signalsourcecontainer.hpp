#include "test_config.h"

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {
class MockSignalSourceContainer : public SignalSourceContainer {
public:
    MockSignalSourceContainer()
        : SignalSourceContainer()
        , m_frame_size(1024)
        , m_processing_state(ProcessingState::IDLE)
        , m_ready_for_processing(false)
    {
        m_processed_data = std::vector<double>(1024, 0.0F);

        DataDimension time_dim("time", m_frame_size);
        time_dim.role = DataDimension::Role::TIME;

        DataDimension chan_dim("channels", 1);
        chan_dim.role = DataDimension::Role::CHANNEL;

        m_dimensions = { time_dim, chan_dim };
    }

    void add_dimension(const DataDimension& dim)
    {
        bool dim_found = false;
        for (auto& dimen : m_dimensions) {
            if (dimen.role == dim.role) {
                dimen = dim;
                dim_found = true;
            }
        }

        if (!dim_found) {
            m_dimensions.push_back(dim);
        }
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
        m_processed_data = data;
    }

    std::vector<DataDimension> get_dimensions() const override
    {
        return m_dimensions;
    }

    u_int64_t get_total_elements() const override
    {
        return m_frame_size;
    }

    MemoryLayout get_memory_layout() const override
    {
        return MemoryLayout::ROW_MAJOR;
    }

    void set_memory_layout(MemoryLayout) override
    {
        // No-op for mock
    }

    u_int64_t get_num_frames() const override
    {
        return 1;
    }

    DataVariant get_region_data(const Region&) const override
    {
        return { m_processed_data };
    }

    void set_region_data(const Region&, const DataVariant&) override
    {
        // No-op for mock
    }

    std::span<const double> get_frame(u_int64_t) const override
    {
        static std::vector<double> empty;
        return {};
    }

    u_int64_t get_frame_size() const override
    {
        return m_frame_size;
    }

    void get_frames(std::span<double>, u_int64_t, u_int64_t) const override
    {
        // No-op for mock
    }

    double get_value_at(const std::vector<u_int64_t>&) const override
    {
        return 0.0;
    }

    void set_value_at(const std::vector<u_int64_t>&, double) override
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

    u_int64_t coordinates_to_linear_index(const std::vector<u_int64_t>&) const override
    {
        return 0;
    }

    std::vector<u_int64_t> linear_index_to_coordinates(u_int64_t) const override
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

    u_int32_t register_dimension_reader(u_int32_t) override
    {
        // No-op for mock
        return 0;
    }

    void unregister_dimension_reader(u_int32_t) override
    {
        // No-op for mock
    }

    bool has_active_readers() const override
    {
        return false;
    }

    void mark_dimension_consumed(u_int32_t, u_int32_t) override
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

    DataVariant& get_processed_data() override
    {
        m_processed_variant = DataVariant(m_processed_data);
        return m_processed_variant;
    }

    DataVariant& get_processed_data() const override
    {
        auto* self = const_cast<MockSignalSourceContainer*>(this);
        self->m_processed_variant = DataVariant(self->m_processed_data);
        return self->m_processed_variant;
    }

    const ContainerDataStructure& get_structure() const override
    {
        return m_data_structure;
    }

    void set_structure(ContainerDataStructure structure) override
    {
        m_data_structure = structure;
        m_ready_for_processing = false; // Reset processing state on structure change
    }

    inline std::span<DataVariant> get_all_processed_data() override
    {
        return { m_processed_data_all };
    }

    inline std::span<const DataVariant> get_all_processed_data() const override
    {
        return { m_processed_data_all };
    }

private:
    std::vector<double> m_processed_data;
    DataVariant m_processed_variant;

    size_t m_frame_size;
    std::vector<DataDimension> m_dimensions;
    std::unordered_map<std::string, RegionGroup> m_region_groups;
    ProcessingState m_processing_state;
    bool m_ready_for_processing;
    std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> m_state_change_callback;
    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;
    ContainerDataStructure m_data_structure;

    std::vector<DataVariant> m_processed_data_all;
};
}
