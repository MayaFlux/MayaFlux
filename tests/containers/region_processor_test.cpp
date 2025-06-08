#include "../test_config.h"

#include "MayaFlux/Kakshya/Processors/RegionProcessors.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {
class MockSignalSourceContainer : public SignalSourceContainer {
public:
    MockSignalSourceContainer()
        : SignalSourceContainer()
        , m_processing_state(ProcessingState::IDLE)
        , m_ready_for_processing(false)
    {
        m_processed_data = std::vector<float>(1024, 0.0f);
        m_frame_size = 1024;
        m_dimensions = {
            DataDimension("time", m_frame_size),
            DataDimension("channels", 1)
        };
    }

    void add_test_region_group(const std::string& name)
    {
        RegionGroup group(name);
        m_region_groups[name] = group;
    }

    void add_test_point_to_group(const std::string& group_name, const RegionPoint& point)
    {
        if (m_region_groups.find(group_name) != m_region_groups.end()) {
            m_region_groups[group_name].add_point(point);
        }
    }

    void set_test_data(const std::vector<float>& data)
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

    DataVariant get_region_data(const RegionPoint& region) const override
    {
        return DataVariant(m_processed_data);
    }

    void set_region_data(const RegionPoint& region, const DataVariant& data) override
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

    bool is_region_loaded(const RegionPoint& region) const override
    {
        return true;
    }

    void load_region(const RegionPoint& region) override
    {
        // No-op for mock
    }

    void unload_region(const RegionPoint& region) override
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

    void register_dimension_reader(u_int32_t dimension_index) override
    {
        // No-op for mock
    }

    void unregister_dimension_reader(u_int32_t dimension_index) override
    {
        // No-op for mock
    }

    bool has_active_readers() const override
    {
        return false;
    }

    void mark_dimension_consumed(u_int32_t dimension_index) override
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
    std::vector<float> m_processed_data;
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

class RegionProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        container = std::make_shared<MockSignalSourceContainer>();
        processor = std::make_unique<RegionOrganizationProcessor>(container);

        container->add_test_region_group("perc");
        container->add_test_region_group("line");

        auto drum_kick = RegionPoint::audio_span(0, 1024, 0, 256, "kick");
        auto drum_snare = RegionPoint::audio_span(0, 1024, 256, 512, "snare");
        auto drum_hihat = RegionPoint::audio_span(0, 1024, 512, 768, "hihat");

        auto line_a = RegionPoint::audio_span(0, 1024, 0, 384, "line_a");
        auto line_b = RegionPoint::audio_span(0, 1024, 384, 768, "line_b");

        container->add_test_point_to_group("perc", drum_kick);
        container->add_test_point_to_group("perc", drum_snare);
        container->add_test_point_to_group("perc", drum_hihat);

        container->add_test_point_to_group("line", line_a);
        container->add_test_point_to_group("line", line_b);
    }

    std::unique_ptr<RegionOrganizationProcessor> processor;
    std::shared_ptr<MockSignalSourceContainer> container;
};

class DynamicRegionProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        container = std::make_shared<MockSignalSourceContainer>();
        processor = std::make_unique<DynamicRegionProcessor>(container);

        container->add_test_region_group("perc");
        container->add_test_region_group("line");

        auto drum_kick = RegionPoint::audio_span(0, 1024, 0, 256, "kick");
        auto drum_snare = RegionPoint::audio_span(0, 1024, 256, 512, "snare");
        auto drum_hihat = RegionPoint::audio_span(0, 1024, 512, 768, "hihat");

        auto line_a = RegionPoint::audio_span(0, 1024, 0, 384, "line_a");
        auto line_b = RegionPoint::audio_span(0, 1024, 384, 768, "line_b");

        container->add_test_point_to_group("perc", drum_kick);
        container->add_test_point_to_group("perc", drum_snare);
        container->add_test_point_to_group("perc", drum_hihat);

        container->add_test_point_to_group("line", line_a);
        container->add_test_point_to_group("line", line_b);
    }

    std::unique_ptr<DynamicRegionProcessor> processor;
    std::shared_ptr<MockSignalSourceContainer> container;
};

TEST_F(RegionProcessorTest, OrganizeContainerData)
{
    processor->organize_container_data(container);

    processor->add_region_group("effects");

    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(RegionProcessorTest, AddSegmentToRegion)
{
    processor->organize_container_data(container);

    std::vector<uint64_t> start_coords = { 100 };
    std::vector<uint64_t> end_coords = { 200 };
    std::unordered_map<std::string, std::any> attributes;
    attributes["gain"] = 0.5;

    processor->add_segment_to_region("perc", 0, start_coords, end_coords, attributes);

    processor->process(container);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(RegionProcessorTest, RegionTransitions)
{
    processor->organize_container_data(container);

    processor->set_region_transition("perc", 0, RegionTransition::CROSSFADE, 50.0);
    processor->set_region_transition("perc", 1, RegionTransition::OVERLAP, 25.0);
    processor->set_region_transition("perc", 2, RegionTransition::IMMEDIATE, 0.0);

    processor->process(container);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(RegionProcessorTest, SelectionPatterns)
{
    processor->organize_container_data(container);

    processor->set_selection_pattern("perc", 0, PointSelectionPattern::SEQUENTIAL);
    processor->set_selection_pattern("perc", 1, PointSelectionPattern::RANDOM);
    processor->set_selection_pattern("line", 0, PointSelectionPattern::ROUND_ROBIN);
    processor->set_selection_pattern("line", 1, PointSelectionPattern::WEIGHTED);

    for (int i = 0; i < 5; i++) {
        processor->process(container);
        EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
    }
}

TEST_F(RegionProcessorTest, RegionLooping)
{
    processor->organize_container_data(container);

    processor->set_region_looping("line", 0, true);

    std::vector<u_int64_t> loop_start = { 100 };
    std::vector<u_int64_t> loop_end = { 400 };
    processor->set_region_looping("line", 1, true, loop_start, loop_end);

    for (int i = 0; i < 5; i++) {
        processor->process(container);
        EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
    }
}

TEST_F(RegionProcessorTest, JumpToRegion)
{
    processor->organize_container_data(container);

    processor->jump_to_region("perc", 1); // Jump to snare
    processor->process(container);

    processor->jump_to_region("line", 0); // Jump to line_a
    processor->process(container);

    std::vector<u_int64_t> position = { 300 };
    processor->jump_to_position(position);
    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, ReorganizationCallback)
{
    processor->organize_container_data(container);

    // Set a reorganization callback that reverses the order of regions
    bool callback_called = false;
    processor->set_reorganization_callback(
        [&callback_called](std::vector<OrganizedRegion>& regions, std::shared_ptr<SignalSourceContainer>) {
            std::reverse(regions.begin(), regions.end());
            callback_called = true;
        });

    processor->trigger_reorganization();

    processor->process(container);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, AutoReorganization)
{
    processor->organize_container_data(container);

    // Set up auto-reorganization based on a counter
    int counter = 0;
    processor->set_auto_reorganization(
        [&counter](const std::vector<OrganizedRegion>&, std::shared_ptr<SignalSourceContainer>) {
            counter++;
            return (counter % 2 == 0);
        });

    bool reorganization_occurred = false;
    for (int i = 0; i < 5; i++) {
        processor->process(container);

        if (i == 1 || i == 3) {
            reorganization_occurred = true;
        }
    }

    EXPECT_TRUE(reorganization_occurred);
    EXPECT_EQ(counter, 5);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, PriorityBasedReorganization)
{
    processor->organize_container_data(container);

    processor->set_reorganization_callback(
        [](std::vector<OrganizedRegion>& regions, std::shared_ptr<SignalSourceContainer>) {
            std::sort(regions.begin(), regions.end(),
                [](const OrganizedRegion& a, const OrganizedRegion& b) {
                    auto a_priority_it = a.attributes.find("priority");
                    auto b_priority_it = b.attributes.find("priority");

                    int a_priority = 0;
                    int b_priority = 0;

                    if (a_priority_it != a.attributes.end()) {
                        try {
                            a_priority = std::any_cast<int>(a_priority_it->second);
                        } catch (const std::bad_any_cast&) {
                        }
                    }

                    if (b_priority_it != b.attributes.end()) {
                        try {
                            b_priority = std::any_cast<int>(b_priority_it->second);
                        } catch (const std::bad_any_cast&) {
                        }
                    }

                    return a_priority > b_priority;
                });
        });

    processor->trigger_reorganization();

    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, ConditionalReorganization)
{
    processor->organize_container_data(container);

    bool condition_met = false;

    processor->set_reorganization_callback(
        [&condition_met](std::vector<OrganizedRegion>& regions, std::shared_ptr<SignalSourceContainer>) {
            if (condition_met) {
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(regions.begin(), regions.end(), g);
            }
        });

    processor->set_auto_reorganization(
        [&condition_met](const std::vector<OrganizedRegion>&, std::shared_ptr<SignalSourceContainer>) {
            return condition_met;
        });

    processor->process(container);

    condition_met = true;
    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, DataDrivenReorganization)
{
    processor->organize_container_data(container);

    std::vector<float> spectral_energy = { 0.2f, 0.8f, 0.5f };

    processor->set_reorganization_callback(
        [&spectral_energy](std::vector<OrganizedRegion>& regions, std::shared_ptr<SignalSourceContainer>) {
            if (regions.size() <= spectral_energy.size()) {
                std::sort(regions.begin(), regions.end(),
                    [&spectral_energy](const OrganizedRegion& a, const OrganizedRegion& b) {
                        size_t a_idx = a.point_index;
                        size_t b_idx = b.point_index;

                        if (a_idx < spectral_energy.size() && b_idx < spectral_energy.size()) {
                            return spectral_energy[a_idx] > spectral_energy[b_idx];
                        }
                        return a_idx < b_idx;
                    });
            }
        });

    processor->trigger_reorganization();

    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, TimeBasedReorganization)
{
    processor->organize_container_data(container);

    auto start_time = std::chrono::steady_clock::now();
    int reorganization_count = 0;

    processor->set_auto_reorganization(
        [&start_time, &reorganization_count](const std::vector<OrganizedRegion>&, std::shared_ptr<SignalSourceContainer>) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time)
                               .count();

            if (elapsed >= 10) { // 10ms interval
                start_time = current_time;
                reorganization_count++;
                return true;
            }
            return false;
        });

    for (int i = 0; i < 3; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        processor->process(container);
    }

    EXPECT_GT(reorganization_count, 0);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(RegionProcessorTest, ChainedProcessors)
{
    auto second_processor = std::make_unique<RegionOrganizationProcessor>(container);

    processor->organize_container_data(container);
    processor->set_selection_pattern("perc", 0, PointSelectionPattern::SEQUENTIAL);

    processor->process(container);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);

    second_processor->organize_container_data(container);
    second_processor->set_selection_pattern("line", 0, PointSelectionPattern::RANDOM);
    second_processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(RegionProcessorTest, ErrorHandling)
{
    std::shared_ptr<SignalSourceContainer> null_container;
    processor->process(null_container);

    auto empty_container = std::make_shared<MockSignalSourceContainer>();
    processor->process(empty_container);
    EXPECT_EQ(empty_container->get_processing_state(), ProcessingState::IDLE);
}

TEST_F(RegionProcessorTest, SpectralRegionProcessing)
{
    std::vector<float> spectral_data(1024);
    for (size_t i = 0; i < spectral_data.size(); i++) {
        if (i > 100 && i < 150) {
            spectral_data[i] = 0.8f;
        } else if (i > 300 && i < 350) {
            spectral_data[i] = 0.6f;
        } else if (i > 700 && i < 750) {
            spectral_data[i] = 0.4f;
        } else {
            spectral_data[i] = 0.1f;
        }
    }
    container->set_test_data(spectral_data);

    container->add_test_region_group("spectral");

    auto formant1 = RegionPoint::audio_span(0, 1024, 100, 150, "formant1");
    formant1.set_attribute("center_frequency", 125.0);
    formant1.set_attribute("bandwidth", 50.0);

    auto formant2 = RegionPoint::audio_span(0, 1024, 300, 350, "formant2");
    formant2.set_attribute("center_frequency", 325.0);
    formant2.set_attribute("bandwidth", 50.0);

    auto formant3 = RegionPoint::audio_span(0, 1024, 700, 750, "formant3");
    formant3.set_attribute("center_frequency", 725.0);
    formant3.set_attribute("bandwidth", 50.0);

    container->add_test_point_to_group("spectral", formant1);
    container->add_test_point_to_group("spectral", formant2);
    container->add_test_point_to_group("spectral", formant3);

    processor->organize_container_data(container);
    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

} // namespace MayaFlux::Test
