#include "../test_config.h"

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class MockSignalSourceContainer : public SignalSourceContainer {
public:
    MockSignalSourceContainer()
        : SignalSourceContainer()
        , m_processing_state(ProcessingState::IDLE)
        , m_ready_for_processing(false)
    {
        m_frame_size = 1024;

        DataDimension time_dim("time", m_frame_size);
        time_dim.role = DataDimension::Role::TIME;

        DataDimension chan_dim("channels", 1);
        chan_dim.role = DataDimension::Role::CHANNEL;

        m_dimensions = { time_dim, chan_dim };
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

    void set_memory_layout(MemoryLayout layout) override
    {
        // No-op for mock
    }

    u_int64_t get_num_frames() const override
    {
        return 1;
    }

    DataVariant get_region_data(const Region& region) const override
    {
        return DataVariant(m_processed_data);
    }

    void set_region_data(const Region& region, const DataVariant& data) override
    {
        // No-op for mock
    }

    std::span<const double> get_frame(u_int64_t frame_index) const override
    {
        static std::vector<double> empty;
        return std::span<const double>(empty);
    }

    virtual u_int64_t get_frame_size() const override
    {
        return 1;
    }

    void get_frames(std::span<double> output, u_int64_t start_frame, u_int64_t num_frames) const override
    {
        // No-op for mock
    }

    double get_value_at(const std::vector<u_int64_t>& coordinates) const override
    {
        return 0.0;
    }

    void set_value_at(const std::vector<u_int64_t>& coordinates, double value) override
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

    bool is_region_loaded(const Region& region) const override
    {
        return true;
    }

    void load_region(const Region& region) override
    {
        // No-op for mock
    }

    void unload_region(const Region& region) override
    {
        // No-op for mock
    }

    u_int64_t coordinates_to_linear_index(const std::vector<u_int64_t>& coordinates) const override
    {
        return 0;
    }

    std::vector<u_int64_t> linear_index_to_coordinates(u_int64_t linear_index) const override
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

    u_int32_t register_dimension_reader(u_int32_t dimension_index) override
    {
        // No-op for mock
        return 0;
    }

    void unregister_dimension_reader(u_int32_t dimension_index) override
    {
        // No-op for mock
    }

    bool has_active_readers() const override
    {
        return false;
    }

    void mark_dimension_consumed(u_int32_t dimension_index, u_int32_t reader_id) override
    {
        // No-op for mock
    }

    bool all_dimensions_consumed() const override
    {
        return true;
    }

    void mark_buffers_for_processing(bool should_process) override
    {
        // No-op for mock
    }

    void mark_buffers_for_removal() override
    {
        // No-op for mock
    }

    ProcessingState get_processing_state() const
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
};

class EnergyAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Simple test signal: ramp from 0 to 1
        test_data.resize(1024);
        for (size_t i = 0; i < test_data.size(); ++i)
            test_data[i] = static_cast<double>(i) / 1023.0f;

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);
        analyzer = std::make_unique<EnergyAnalyzer>(256, 128);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::unique_ptr<EnergyAnalyzer> analyzer;
};

TEST_F(EnergyAnalyzerTest, CalculateRMSEnergy)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_FALSE(values.empty());
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculatePeakEnergy)
{
    analyzer->set_parameter("method", "peak");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_FALSE(values.empty());
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, CalculateSpectralEnergy)
{
    analyzer->set_parameter("method", "spectral");
    analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result));
    const auto& values = std::get<std::vector<double>>(result);

    EXPECT_FALSE(values.empty());
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

TEST_F(EnergyAnalyzerTest, EnergyRegionsOutput)
{
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result));
    const auto& group = std::get<RegionGroup>(result);

    EXPECT_FALSE(group.regions.empty());
}

TEST_F(EnergyAnalyzerTest, ThresholdConfiguration)
{
    analyzer->set_energy_thresholds(0.01, 0.05, 0.1, 0.5);
    analyzer->set_parameter("method", "rms");
    analyzer->set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

    AnalyzerInput input = container;
    auto result = analyzer->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result));
}

TEST_F(EnergyAnalyzerTest, InvalidContainerThrows)
{
    std::shared_ptr<MockSignalSourceContainer> empty_container = std::make_shared<MockSignalSourceContainer>();
    AnalyzerInput input = empty_container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

TEST_F(EnergyAnalyzerTest, InvalidMethodThrows)
{
    analyzer->set_parameter("method", "not_a_method");
    AnalyzerInput input = container;

    EXPECT_THROW(analyzer->apply_operation(input), std::invalid_argument);
}

} // namespace MayaFlux::Test
