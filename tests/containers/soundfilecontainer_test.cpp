#include "../test_config.h"

#include "MayaFlux/Kakshya/DataProcessingChain.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class SoundFileContainerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 4 frames, 2 channels, 8 elements (interleaved: LRLRLRLR)
        test_data = std::vector<double> { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8 };
        container = std::make_shared<SoundFileContainer>();
        container->get_structure().organization = OrganizationStrategy::INTERLEAVED;
        container->setup(4, 48000, 2);

        container->set_raw_data({ test_data });
    }

    std::vector<double> test_data;
    std::shared_ptr<SoundFileContainer> container;
};

TEST_F(SoundFileContainerTest, BasicConstructionAndSetup)
{
    EXPECT_EQ(container->get_num_frames(), 4);
    EXPECT_EQ(container->get_sample_rate(), 48000);
    EXPECT_EQ(container->get_num_channels(), 2);
    EXPECT_EQ(container->get_total_elements(), 8);

    auto dims = container->get_dimensions();
    ASSERT_EQ(dims.size(), 2);
    EXPECT_EQ(dims[0].size, 4);
    EXPECT_EQ(dims[1].size, 2);

    EXPECT_TRUE(container->has_data());
    EXPECT_EQ(container->get_memory_layout(), MemoryLayout::ROW_MAJOR);

    auto& structure = container->get_structure();
    EXPECT_EQ(structure.organization, OrganizationStrategy::INTERLEAVED);
}

TEST_F(SoundFileContainerTest, FrameAndCoordinateAccess)
{
    auto frame0 = container->get_frame(0);
    ASSERT_EQ(frame0.size(), 2);
    EXPECT_DOUBLE_EQ(frame0[0], 0.1);
    EXPECT_DOUBLE_EQ(frame0[1], 0.2);

    auto frame2 = container->get_frame(2);
    ASSERT_EQ(frame2.size(), 2);
    EXPECT_DOUBLE_EQ(frame2[0], 0.5);
    EXPECT_DOUBLE_EQ(frame2[1], 0.6);

    auto frame4 = container->get_frame(4);
    EXPECT_TRUE(frame4.empty());

    std::vector<uint64_t> coords = { 1, 1 };
    double val = container->get_value_at(coords);
    EXPECT_DOUBLE_EQ(val, 0.4);

    container->set_value_at(coords, 1.23);
    EXPECT_DOUBLE_EQ(container->get_value_at(coords), 1.23);

    uint64_t lin = container->coordinates_to_linear_index({ 2, 0 });
    EXPECT_EQ(lin, 4);
    auto coords2 = container->linear_index_to_coordinates(7);
    ASSERT_EQ(coords2.size(), 2);
    EXPECT_EQ(coords2[0], 3);
    EXPECT_EQ(coords2[1], 1);
}

TEST_F(SoundFileContainerTest, RegionDataAccess)
{
    container->set_value_at({ 1, 1 }, 1.23);

    Region region(std::vector<uint64_t>({ 1, 0 }), std::vector<uint64_t>({ 2, 1 }));

    auto region_data = container->get_region_data(region);
    ASSERT_FALSE(region_data.empty());

    ASSERT_EQ(region_data.size(), 1);
    auto vec = std::get<std::vector<double>>(region_data[0]);

    // Extraction order: (1,0), (2,0), (1,1), (2,1)
    ASSERT_EQ(vec.size(), 4);
    EXPECT_DOUBLE_EQ(vec[0], 0.3); // (1,0)
    EXPECT_DOUBLE_EQ(vec[1], 1.23); // (2,0)
    EXPECT_DOUBLE_EQ(vec[2], 0.5); // (1,1)
    EXPECT_DOUBLE_EQ(vec[3], 0.6); // (2,1)

    std::vector<double> new_vals { 9.0, 8.0, 7.0, 6.0 };
    std::vector<DataVariant> new_data { new_vals };
    container->set_region_data(region, new_data);

    auto updated = container->get_region_data(region);
    ASSERT_FALSE(updated.empty());
    auto updated_vec = std::get<std::vector<double>>(updated[0]);
    EXPECT_DOUBLE_EQ(updated_vec[0], 9.0);
    EXPECT_DOUBLE_EQ(updated_vec[3], 6.0);
}

TEST_F(SoundFileContainerTest, RegionGroupManagement)
{
    RegionGroup group("test_group");
    group.add_region(Region::time_point(1, "onset"));
    container->add_region_group(group);

    auto retrieved = container->get_region_group("test_group");
    EXPECT_EQ(retrieved.name, "test_group");
    EXPECT_EQ(retrieved.regions.size(), 1);

    container->remove_region_group("test_group");
    auto missing = container->get_region_group("test_group");
    EXPECT_TRUE(missing.name.empty());

    auto all = container->get_all_region_groups();
    EXPECT_TRUE(all.empty());
}

TEST_F(SoundFileContainerTest, StateAndProcessing)
{
    container->update_processing_state(ProcessingState::READY);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::READY);

    container->mark_ready_for_processing(false);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::IDLE);

    bool called = false;
    container->register_state_change_callback([&](std::shared_ptr<SignalSourceContainer>, ProcessingState state) {
        if (state == ProcessingState::PROCESSED)
            called = true;
    });
    container->update_processing_state(ProcessingState::PROCESSED);
    EXPECT_TRUE(called);

    container->unregister_state_change_callback();
}

TEST_F(SoundFileContainerTest, MemoryLayoutSwitching)
{
    container->set_memory_layout(MemoryLayout::COLUMN_MAJOR);
    EXPECT_EQ(container->get_memory_layout(), MemoryLayout::COLUMN_MAJOR);

    container->set_memory_layout(MemoryLayout::ROW_MAJOR);
    EXPECT_EQ(container->get_memory_layout(), MemoryLayout::ROW_MAJOR);
}

TEST_F(SoundFileContainerTest, ProcessedDataAccess)
{
    auto& processed_data = container->get_processed_data();

    if (!processed_data.empty()) {
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(processed_data[0]));
    }
}

TEST_F(SoundFileContainerTest, ClearAndReset)
{
    container->clear();
    EXPECT_EQ(container->get_num_frames(), 0);
    EXPECT_EQ(container->get_num_channels(), 2);
    EXPECT_FALSE(container->has_data());
    EXPECT_EQ(container->get_total_elements(), 0);

    auto& processed_data = container->get_processed_data();
    EXPECT_TRUE(processed_data.empty() || std::visit([](const auto& vec) { return vec.empty(); }, processed_data[0]));
}

TEST_F(SoundFileContainerTest, ProcessorsAndReaders)
{
    container->create_default_processor();
    auto proc = container->get_default_processor();
    EXPECT_TRUE(proc != nullptr);

    auto chain = std::make_shared<DataProcessingChain>();
    container->set_processing_chain(chain);
    EXPECT_EQ(container->get_processing_chain(), chain);

    auto id = container->register_dimension_reader(0);
    EXPECT_TRUE(container->has_active_readers());

    container->mark_dimension_consumed(0, id);
    EXPECT_TRUE(container->all_dimensions_consumed());

    container->unregister_dimension_reader(0);
    EXPECT_FALSE(container->has_active_readers());
}

TEST_F(SoundFileContainerTest, DurationAndTimeConversion)
{
    EXPECT_DOUBLE_EQ(container->get_duration_seconds(), 4.0 / 48000.0);
    EXPECT_EQ(container->time_to_position(0.0000416667), 2);
    EXPECT_NEAR(container->position_to_time(2), 2.0 / 48000.0, 1e-8);
}

TEST_F(SoundFileContainerTest, OrganizationStrategyHandling)
{
    std::vector<DataVariant> planar_data;
    planar_data.push_back(std::vector<double> { 0.1, 0.3, 0.5, 0.7 });
    planar_data.push_back(std::vector<double> { 0.2, 0.4, 0.6, 0.8 });

    auto planar_container = std::make_shared<SoundFileContainer>();
    planar_container->setup(4, 48000, 2);

    auto structure = planar_container->get_structure();
    structure.organization = OrganizationStrategy::PLANAR;
    planar_container->set_structure(structure);

    planar_container->set_raw_data(planar_data);

    EXPECT_EQ(planar_container->get_structure().organization, OrganizationStrategy::PLANAR);

    auto frame0 = planar_container->get_frame(0);
    ASSERT_EQ(frame0.size(), 2);
    EXPECT_DOUBLE_EQ(frame0[0], 0.1);
    EXPECT_DOUBLE_EQ(frame0[1], 0.2);
}

// Commented out tests that depend on read position functionality
// which may need further updates for multi-channel position vectors

/* TEST_F(SoundFileContainerTest, SequentialReadAndPeek)
{
    container->reset_read_position();
    std::vector<double> out(4);
    uint64_t read = container->read_sequential(out, 4);
    EXPECT_EQ(read, 4);
    EXPECT_DOUBLE_EQ(out[0], 0.1);

    container->reset_read_position();
    std::vector<double> peek_out(2);
    uint64_t peeked = container->peek_sequential(peek_out, 2, 1);
    EXPECT_EQ(peeked, 2);
    EXPECT_DOUBLE_EQ(peek_out[0], 0.3);

    auto read_pos = container->get_read_position();
    EXPECT_TRUE(!read_pos.empty());

    container->set_read_position({4, 4}); // Set both channels to end
    EXPECT_TRUE(container->is_at_end());

    container->reset_read_position();
    auto reset_pos = container->get_read_position();
    EXPECT_TRUE(std::all_of(reset_pos.begin(), reset_pos.end(),
                           [](uint64_t pos) { return pos == 0; }));
} */

/* TEST_F(SoundFileContainerTest, LoopingBehavior)
{
    container->set_looping(true);
    Region loop_region = Region::time_span(1, 2);
    container->set_loop_region(loop_region);
    container->set_read_position({2, 2});

    std::vector<uint64_t> advance_frames = {2, 2};
    container->advance_read_position(advance_frames);

    auto pos = container->get_read_position();
    EXPECT_EQ(pos[0], 1); // Should loop back

    auto lr = container->get_loop_region();
    EXPECT_EQ(lr.start_coordinates[0], 1);
    EXPECT_EQ(lr.end_coordinates[0], 2);

    container->set_looping(false);
    container->set_read_position({3, 3});
    container->advance_read_position({1, 1});
    EXPECT_TRUE(container->is_at_end());
} */

} // namespace MayaFlux::Test
