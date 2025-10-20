#include <algorithm>

#include "../mock_signalsourcecontainer.hpp"

#include "MayaFlux/Kakshya/Processors/RegionProcessors.hpp"

namespace MayaFlux::Test {

class RegionProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        container = std::make_shared<MockSignalSourceContainer>();
        processor = std::make_unique<RegionOrganizationProcessor>(container);

        container->add_test_region_group("perc");
        container->add_test_region_group("line");

        auto drum_kick = Region::audio_span(0, 1024, 0, 256, "kick");
        auto drum_snare = Region::audio_span(0, 1024, 256, 512, "snare");
        auto drum_hihat = Region::audio_span(0, 1024, 512, 768, "hihat");

        auto line_a = Region::audio_span(0, 1024, 0, 384, "line_a");
        auto line_b = Region::audio_span(0, 1024, 384, 768, "line_b");

        container->add_test_region_to_group("perc", drum_kick);
        container->add_test_region_to_group("perc", drum_snare);
        container->add_test_region_to_group("perc", drum_hihat);

        container->add_test_region_to_group("line", line_a);
        container->add_test_region_to_group("line", line_b);
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

        auto drum_kick = Region::audio_span(0, 1024, 0, 256, "kick");
        auto drum_snare = Region::audio_span(0, 1024, 256, 512, "snare");
        auto drum_hihat = Region::audio_span(0, 1024, 512, 768, "hihat");

        auto line_a = Region::audio_span(0, 1024, 0, 384, "line_a");
        auto line_b = Region::audio_span(0, 1024, 384, 768, "line_b");

        container->add_test_region_to_group("perc", drum_kick);
        container->add_test_region_to_group("perc", drum_snare);
        container->add_test_region_to_group("perc", drum_hihat);

        container->add_test_region_to_group("line", line_a);
        container->add_test_region_to_group("line", line_b);
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

    processor->set_selection_pattern("perc", 0, RegionSelectionPattern::SEQUENTIAL);
    processor->set_selection_pattern("perc", 1, RegionSelectionPattern::RANDOM);
    processor->set_selection_pattern("line", 0, RegionSelectionPattern::ROUND_ROBIN);
    processor->set_selection_pattern("line", 1, RegionSelectionPattern::WEIGHTED);

    for (int i = 0; i < 5; i++) {
        processor->process(container);
        EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
    }
}

TEST_F(RegionProcessorTest, RegionLooping)
{
    processor->organize_container_data(container);

    processor->set_region_looping("line", 0, true);

    std::vector<uint64_t> loop_start = { 100 };
    std::vector<uint64_t> loop_end = { 400 };
    processor->set_region_looping("line", 1, true, loop_start, loop_end);

    for (int i = 0; i < 5; i++) {
        processor->process(container);
        EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
    }
}

TEST_F(RegionProcessorTest, JumpToRegion)
{
    processor->organize_container_data(container);

    processor->jump_to_region("perc", 1);
    processor->process(container);

    processor->jump_to_region("line", 0);
    processor->process(container);

    std::vector<uint64_t> position = { 300 };
    processor->jump_to_position(position);
    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

TEST_F(DynamicRegionProcessorTest, ReorganizationCallback)
{
    processor->organize_container_data(container);

    bool callback_called = false;
    processor->set_reorganization_callback(
        [&callback_called](std::vector<OrganizedRegion>& regions, const std::shared_ptr<SignalSourceContainer>&) {
            std::ranges::reverse(regions);
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

    int counter = 0;
    processor->set_auto_reorganization(
        [&counter](const std::vector<OrganizedRegion>&, const std::shared_ptr<SignalSourceContainer>&) {
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
        [](std::vector<OrganizedRegion>& regions, const std::shared_ptr<SignalSourceContainer>&) {
            std::ranges::sort(regions,
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
        [&condition_met](std::vector<OrganizedRegion>& regions, const std::shared_ptr<SignalSourceContainer>&) {
            if (condition_met) {
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(regions.begin(), regions.end(), g);
            }
        });

    processor->set_auto_reorganization(
        [&condition_met](const std::vector<OrganizedRegion>&, const std::shared_ptr<SignalSourceContainer>&) {
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

    std::vector<float> spectral_energy = { 0.2F, 0.8F, 0.5F };

    processor->set_reorganization_callback(
        [&spectral_energy](std::vector<OrganizedRegion>& regions, const std::shared_ptr<SignalSourceContainer>&) {
            if (regions.size() <= spectral_energy.size()) {
                std::ranges::sort(regions,
                    [&spectral_energy](const OrganizedRegion& a, const OrganizedRegion& b) {
                        size_t a_idx = a.region_index;
                        size_t b_idx = b.region_index;

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
        [&start_time, &reorganization_count](const std::vector<OrganizedRegion>&, const std::shared_ptr<SignalSourceContainer>&) {
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
    processor->set_selection_pattern("perc", 0, RegionSelectionPattern::SEQUENTIAL);

    processor->process(container);
    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);

    second_processor->organize_container_data(container);
    second_processor->set_selection_pattern("line", 0, RegionSelectionPattern::RANDOM);
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
    std::vector<double> spectral_data(1024);
    for (size_t i = 0; i < spectral_data.size(); i++) {
        if (i > 100 && i < 150) {
            spectral_data[i] = 0.8F;
        } else if (i > 300 && i < 350) {
            spectral_data[i] = 0.6F;
        } else if (i > 700 && i < 750) {
            spectral_data[i] = 0.4F;
        } else {
            spectral_data[i] = 0.1F;
        }
    }
    container->set_test_data(spectral_data);

    container->add_test_region_group("spectral");

    auto formant1 = Region::audio_span(0, 1024, 100, 150, "formant1");
    formant1.set_attribute("center_frequency", 125.0);
    formant1.set_attribute("bandwidth", 50.0);

    auto formant2 = Region::audio_span(0, 1024, 300, 350, "formant2");
    formant2.set_attribute("center_frequency", 325.0);
    formant2.set_attribute("bandwidth", 50.0);

    auto formant3 = Region::audio_span(0, 1024, 700, 750, "formant3");
    formant3.set_attribute("center_frequency", 725.0);
    formant3.set_attribute("bandwidth", 50.0);

    container->add_test_region_to_group("spectral", formant1);
    container->add_test_region_to_group("spectral", formant2);
    container->add_test_region_to_group("spectral", formant3);

    processor->organize_container_data(container);
    processor->process(container);

    EXPECT_EQ(container->get_processing_state(), ProcessingState::PROCESSED);
}

} // namespace MayaFlux::Test
