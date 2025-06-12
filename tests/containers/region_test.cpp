#include "../test_config.h"

#include "MayaFlux/Kakshya/KakshyaUtils.hpp"
#include "MayaFlux/Kakshya/Region.hpp"
#include <chrono>
#include <thread>

using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class RegionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        region = Kakshya::Region({ 100 });

        audio_span = Region::audio_span(0, 1000, 0, 1, "test_audio");

        time_span = Region::time_span(50, 150, "test_span");
        time_span.set_attribute("energy", 0.75);
        time_span.set_attribute("frequency", 440.0);

        std::vector<u_int64_t> start_index = { 10, 20, 30 };
        std::vector<u_int64_t> end_index = { 50, 60, 70 };
        multi_dim = Kakshya::Region(start_index, end_index);
    }

    Region region;
    Region audio_span;
    Region time_span;
    Region multi_dim;
};

TEST_F(RegionTest, BasicConstruction)
{
    EXPECT_EQ(region.start_coordinates.size(), 1);
    EXPECT_EQ(region.end_coordinates.size(), 1);
    EXPECT_EQ(region.start_coordinates[0], 100);
    EXPECT_EQ(region.end_coordinates[0], 100);
    EXPECT_TRUE(region.is_point());

    EXPECT_FALSE(audio_span.is_point());
    EXPECT_EQ(audio_span.start_coordinates[0], 0);
    EXPECT_EQ(audio_span.end_coordinates[0], 1000);
    EXPECT_EQ(audio_span.start_coordinates[1], 0);
    EXPECT_EQ(audio_span.end_coordinates[1], 1);

    Region empty;
    EXPECT_TRUE(empty.start_coordinates.empty());
    EXPECT_TRUE(empty.end_coordinates.empty());
}

TEST_F(RegionTest, StaticFactoryMethods)
{
    auto time_point = Region::time_point(500, "onset");
    EXPECT_TRUE(time_point.is_point());
    EXPECT_EQ(time_point.start_coordinates[0], 500);
    EXPECT_EQ(time_point.get_label(), "onset");

    auto audio_point = Region::audio_point(100, 1, "transient");
    EXPECT_EQ(audio_point.start_coordinates[0], 100);
    EXPECT_EQ(audio_point.start_coordinates[1], 1);
    EXPECT_EQ(audio_point.get_label(), "transient");
    auto type = audio_point.get_attribute<std::string>("type");
    EXPECT_TRUE(type.has_value());

    if (type.has_value()) {
        EXPECT_EQ(*type, "audio_point");
    }

    auto img_rect = Region::image_rect(10, 20, 100, 200, "roi");
    EXPECT_EQ(img_rect.start_coordinates[0], 10);
    EXPECT_EQ(img_rect.start_coordinates[1], 20);
    EXPECT_EQ(img_rect.end_coordinates[0], 100);
    EXPECT_EQ(img_rect.end_coordinates[1], 200);
    EXPECT_EQ(img_rect.get_label(), "roi");

    auto video_region = Region::video_region(0, 30, 50, 60, 150, 160, "scene");
    EXPECT_EQ(video_region.start_coordinates.size(), 3);
    EXPECT_EQ(video_region.start_coordinates[0], 0);
    EXPECT_EQ(video_region.start_coordinates[1], 50); // x1
    EXPECT_EQ(video_region.start_coordinates[2], 60); // y1
    EXPECT_EQ(video_region.end_coordinates[0], 30);
    EXPECT_EQ(video_region.end_coordinates[1], 150); // x2
    EXPECT_EQ(video_region.end_coordinates[2], 160); // y2
}

TEST_F(RegionTest, AttributeManagement)
{
    region.set_attribute("gain", 0.8);
    region.set_attribute("label", std::string("test_region"));
    region.set_attribute("active", true);

    auto gain = region.get_attribute<double>("gain");
    EXPECT_TRUE(gain.has_value());
    EXPECT_DOUBLE_EQ(*gain, 0.8);

    auto label = region.get_attribute<std::string>("label");
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "test_region");

    auto active = region.get_attribute<bool>("active");
    EXPECT_TRUE(active.has_value());
    EXPECT_TRUE(*active);

    auto missing = region.get_attribute<int>("missing");
    EXPECT_FALSE(missing.has_value());

    auto wrong_type = region.get_attribute<int>("gain");
    EXPECT_FALSE(wrong_type.has_value());

    region.set_label("convenience_label");
    EXPECT_EQ(region.get_label(), "convenience_label");
}

TEST_F(RegionTest, GeometryOperations)
{
    EXPECT_EQ(time_span.get_span(0), 101); // 150 - 50 + 1
    EXPECT_EQ(audio_span.get_span(0), 1001); // 1000 - 0 + 1
    EXPECT_EQ(audio_span.get_span(1), 2); // 1 - 0 + 1

    EXPECT_EQ(region.get_volume(), 1);
    EXPECT_EQ(time_span.get_volume(), 101);
    EXPECT_EQ(audio_span.get_volume(), 2002); // 1001 * 2
    EXPECT_EQ(multi_dim.get_volume(), 41 * 41 * 41); // (50-10+1) * (60-20+1) * (70-30+1)

    EXPECT_EQ(time_span.get_duration(0), 101);
    EXPECT_EQ(audio_span.get_duration(1), 2);

    EXPECT_EQ(region.get_span(1), 0);
    EXPECT_EQ(region.get_duration(5), 0);
}

TEST_F(RegionTest, ContainmentAndOverlap)
{
    EXPECT_TRUE(region.contains({ 100 }));
    EXPECT_FALSE(region.contains({ 99 }));
    EXPECT_FALSE(region.contains({ 101 }));

    EXPECT_TRUE(time_span.contains({ 75 }));
    EXPECT_TRUE(time_span.contains({ 50 }));
    EXPECT_TRUE(time_span.contains({ 150 }));
    EXPECT_FALSE(time_span.contains({ 49 }));
    EXPECT_FALSE(time_span.contains({ 151 }));

    EXPECT_TRUE(multi_dim.contains({ 25, 40, 50 }));
    EXPECT_TRUE(multi_dim.contains({ 10, 20, 30 }));
    EXPECT_TRUE(multi_dim.contains({ 50, 60, 70 }));
    EXPECT_FALSE(multi_dim.contains({ 9, 40, 50 }));
    EXPECT_FALSE(multi_dim.contains({ 25, 19, 50 }));
    EXPECT_FALSE(multi_dim.contains({ 25, 40, 71 }));

    EXPECT_FALSE(region.contains({ 100, 200 }));
    EXPECT_FALSE(multi_dim.contains({ 25, 40 }));

    Region overlap1(std::vector<u_int64_t>({ 75 }), std::vector<u_int64_t>({ 125 }));
    Region overlap2(std::vector<u_int64_t>({ 125 }), std::vector<u_int64_t>({ 175 }));
    Region no_overlap(std::vector<u_int64_t>({ 200 }), std::vector<u_int64_t>({ 250 }));

    EXPECT_TRUE(time_span.overlaps(overlap1));
    EXPECT_TRUE(time_span.overlaps(overlap2));
    EXPECT_FALSE(time_span.overlaps(no_overlap));

    EXPECT_TRUE(time_span.overlaps(time_span));

    Region multi_overlap(std::vector<u_int64_t>({ 40, 50, 60 }), std::vector<u_int64_t>({ 80, 90, 100 }));
    EXPECT_TRUE(multi_dim.overlaps(multi_overlap));

    Region multi_no_overlap(std::vector<u_int64_t>({ 100, 100, 100 }), std::vector<u_int64_t>({ 200, 200, 200 }));
    EXPECT_FALSE(multi_dim.overlaps(multi_no_overlap));
}

TEST_F(RegionTest, Transformations)
{
    auto translated = time_span.translate({ 10 });
    EXPECT_EQ(translated.start_coordinates[0], 60); // 50 + 10
    EXPECT_EQ(translated.end_coordinates[0], 160); // 150 + 10

    auto multi_translated = multi_dim.translate({ 5, -5, 10 });
    EXPECT_EQ(multi_translated.start_coordinates[0], 15); // 10 + 5
    EXPECT_EQ(multi_translated.start_coordinates[1], 15); // 20 - 5
    EXPECT_EQ(multi_translated.start_coordinates[2], 40); // 30 + 10
    EXPECT_EQ(multi_translated.end_coordinates[0], 55); // 50 + 5
    EXPECT_EQ(multi_translated.end_coordinates[1], 55); // 60 - 5
    EXPECT_EQ(multi_translated.end_coordinates[2], 80); // 70 + 10

    auto negative_translate = Region::time_span(5, 10).translate({ -10 });
    EXPECT_EQ(negative_translate.start_coordinates[0], 0);

    auto scaled = time_span.scale({ 2.0 });
    u_int64_t center = (50 + 150) / 2; // 100
    u_int64_t half_span = (150 - 50) / 2; // 50
    u_int64_t new_half_span = static_cast<u_int64_t>(half_span * 2.0); // 100
    EXPECT_EQ(scaled.start_coordinates[0], center - new_half_span); // 0
    EXPECT_EQ(scaled.end_coordinates[0], center + new_half_span); // 200

    auto scaled_down = time_span.scale({ 0.5 });
    u_int64_t new_half_span_down = static_cast<u_int64_t>(half_span * 0.5); // 25
    EXPECT_EQ(scaled_down.start_coordinates[0], center - new_half_span_down); // 75
    EXPECT_EQ(scaled_down.end_coordinates[0], center + new_half_span_down); // 125

    auto multi_scaled = multi_dim.scale({ 2.0, 0.5, 1.0 });
    EXPECT_GT(multi_scaled.get_span(0), multi_dim.get_span(0)); // Should be larger
    EXPECT_LT(multi_scaled.get_span(1), multi_dim.get_span(1)); // Should be smaller
    EXPECT_EQ(multi_scaled.get_span(2), multi_dim.get_span(2)); // Should be same
}

TEST_F(RegionTest, EqualityOperators)
{
    Region identical({ 100 });
    Region different({ 101 });

    EXPECT_TRUE(region == identical);
    EXPECT_FALSE(region == different);
    EXPECT_FALSE(region != identical);
    EXPECT_TRUE(region != different);

    Region span_identical = Region::time_span(50, 150);
    Region span_different = Region::time_span(50, 151);

    EXPECT_TRUE(time_span == span_identical);
    EXPECT_FALSE(time_span == span_different);

    Region multi_identical(std::vector<u_int64_t>({ 10, 20, 30 }), std::vector<u_int64_t>({ 50, 60, 70 }));
    Region multi_different(std::vector<u_int64_t>({ 10, 20, 30 }), std::vector<u_int64_t>({ 50, 60, 71 }));

    EXPECT_TRUE(multi_dim == multi_identical);
    EXPECT_FALSE(multi_dim == multi_different);
}

TEST_F(RegionTest, DSPSpecificUseCases)
{
    auto onset_region = Region::time_point(1000, "onset");
    onset_region.set_attribute("energy", 0.85);
    onset_region.set_attribute("spectral_centroid", 2500.0);
    onset_region.set_attribute("detected_by", std::string("peak_picker"));

    EXPECT_EQ(onset_region.get_label(), "onset");
    auto energy = onset_region.get_attribute<double>("energy");
    EXPECT_TRUE(energy.has_value());
    EXPECT_DOUBLE_EQ(*energy, 0.85);

    auto spectral_region = Region::audio_span(0, 2048, 100, 200, "formant");
    spectral_region.set_attribute("center_frequency", 1000.0);
    spectral_region.set_attribute("bandwidth", 100.0);
    spectral_region.set_attribute("q_factor", 10.0);

    auto center_freq = spectral_region.get_attribute<double>("center_frequency");
    EXPECT_TRUE(center_freq.has_value());
    EXPECT_DOUBLE_EQ(*center_freq, 1000.0);

    auto zero_crossing = Region::time_span(500, 600, "zero_crossing_cluster");
    zero_crossing.set_attribute("crossing_rate", 15.5);
    zero_crossing.set_attribute("rms_level", -20.0);

    auto crossing_rate = zero_crossing.get_attribute<double>("crossing_rate");
    EXPECT_TRUE(crossing_rate.has_value());
    EXPECT_DOUBLE_EQ(*crossing_rate, 15.5);
}

class RegionSegmentTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        source_region = Region::audio_span(0, 1000, 0, 1, "audio_segment");
        segment = RegionSegment(source_region);

        custom_segment = RegionSegment(
            source_region,
            { 100, 0 }, // offset
            { 500, 2 } // size
        );
    }

    Region source_region;
    RegionSegment segment;
    RegionSegment custom_segment;
};

TEST_F(RegionSegmentTest, BasicConstruction)
{
    EXPECT_EQ(segment.source_region, source_region);
    EXPECT_EQ(segment.offset_in_region.size(), 2);
    EXPECT_EQ(segment.segment_size.size(), 2);
    EXPECT_EQ(segment.current_position.size(), 2);

    EXPECT_EQ(segment.offset_in_region[0], 0);
    EXPECT_EQ(segment.offset_in_region[1], 0);
    EXPECT_EQ(segment.segment_size[0], 1001); // span of dimension 0
    EXPECT_EQ(segment.segment_size[1], 2); // span of dimension 1

    EXPECT_FALSE(segment.is_cached);
    EXPECT_FALSE(segment.is_active);
    EXPECT_EQ(segment.state, RegionState::IDLE);

    EXPECT_EQ(custom_segment.offset_in_region[0], 100);
    EXPECT_EQ(custom_segment.offset_in_region[1], 0);
    EXPECT_EQ(custom_segment.segment_size[0], 500);
    EXPECT_EQ(custom_segment.segment_size[1], 2);
}

TEST_F(RegionSegmentTest, VolumeAndContainment)
{
    EXPECT_EQ(segment.get_total_elements(), 1001 * 2); // 2002
    EXPECT_EQ(custom_segment.get_total_elements(), 500 * 2); // 1000

    EXPECT_TRUE(custom_segment.contains_position({ 200, 1 }));
    EXPECT_TRUE(custom_segment.contains_position({ 100, 0 }));
    EXPECT_TRUE(custom_segment.contains_position({ 599, 1 })); // at end (100 + 500 - 1)

    EXPECT_FALSE(custom_segment.contains_position({ 99, 0 }));
    EXPECT_FALSE(custom_segment.contains_position({ 600, 0 }));
    EXPECT_FALSE(custom_segment.contains_position({ 200, 2 }));

    EXPECT_FALSE(custom_segment.contains_position({ 200 }));
    EXPECT_FALSE(custom_segment.contains_position({ 200, 1, 0 }));
}

TEST_F(RegionSegmentTest, StateManagement)
{
    EXPECT_EQ(segment.state, RegionState::IDLE);
    EXPECT_FALSE(segment.is_active);

    segment.mark_active();
    EXPECT_TRUE(segment.is_active);
    EXPECT_EQ(segment.state, RegionState::ACTIVE);

    segment.mark_inactive();
    EXPECT_FALSE(segment.is_active);
    EXPECT_EQ(segment.state, RegionState::IDLE);

    segment.state = RegionState::LOADING;
    EXPECT_EQ(segment.state, RegionState::LOADING);

    segment.state = RegionState::READY;
    EXPECT_EQ(segment.state, RegionState::READY);

    segment.state = RegionState::TRANSITIONING;
    EXPECT_EQ(segment.state, RegionState::TRANSITIONING);

    segment.state = RegionState::UNLOADING;
    EXPECT_EQ(segment.state, RegionState::UNLOADING);
}

TEST_F(RegionSegmentTest, CacheManagement)
{
    EXPECT_FALSE(segment.is_cached);
    EXPECT_EQ(segment.get_cache_age_seconds(), -1.0);

    DataVariant test_data = std::vector<double> { 1.0, 2.0, 3.0 };
    segment.mark_cached(test_data);

    EXPECT_TRUE(segment.is_cached);
    EXPECT_EQ(segment.state, RegionState::READY);
    EXPECT_GE(segment.get_cache_age_seconds(), 0.0);
    EXPECT_FALSE(segment.cache.is_dirty);

    segment.cache.mark_accessed();
    EXPECT_EQ(segment.cache.access_count, 1);
    segment.cache.mark_accessed();
    EXPECT_EQ(segment.cache.access_count, 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GT(segment.get_cache_age_seconds(), 0.01);

    segment.cache.mark_dirty();
    EXPECT_TRUE(segment.cache.is_dirty);

    segment.clear_cache();
    EXPECT_FALSE(segment.is_cached);
    EXPECT_EQ(segment.state, RegionState::IDLE);
    EXPECT_EQ(segment.get_cache_age_seconds(), -1.0);
}

TEST_F(RegionSegmentTest, PositionManagement)
{
    EXPECT_TRUE(std::all_of(segment.current_position.begin(), segment.current_position.end(),
        [](u_int64_t pos) { return pos == 0; }));

    segment.current_position[0] = 50;
    segment.current_position[1] = 1;
    segment.reset_position();
    EXPECT_TRUE(std::all_of(segment.current_position.begin(), segment.current_position.end(),
        [](u_int64_t pos) { return pos == 0; }));

    EXPECT_TRUE(segment.advance_position(10, 0)); // advance 10 steps in dimension 0
    EXPECT_EQ(segment.current_position[0], 10);
    EXPECT_EQ(segment.current_position[1], 0);

    // Advance to boundary of dimension 0
    EXPECT_TRUE(segment.advance_position(segment.segment_size[0] - 11, 0)); // to end - 1
    EXPECT_EQ(segment.current_position[0], segment.segment_size[0] - 1);

    // Advance past boundary (should overflow to next dimension)
    EXPECT_TRUE(segment.advance_position(1, 0));
    EXPECT_EQ(segment.current_position[0], 0);
    EXPECT_EQ(segment.current_position[1], 1);

    segment.reset_position();
    for (int i = 0; i < 100; ++i) {
        if (!segment.advance_position(1, 0)) {
            break;
        }
    }
    EXPECT_GT(segment.current_position[0], 0);

    segment.current_position[1] = segment.segment_size[1] - 1;
    EXPECT_FALSE(segment.advance_position(segment.segment_size[0], 0)); // Should reach end
    EXPECT_TRUE(segment.is_at_end());

    EXPECT_FALSE(segment.advance_position(1, 10)); // dimension 10 doesn't exist
}

TEST_F(RegionSegmentTest, ProcessingMetadata)
{
    segment.set_processing_metadata("gain", 0.8);
    segment.set_processing_metadata("frequency", 440.0);
    segment.set_processing_metadata("active", true);
    segment.set_processing_metadata("name", std::string("test_segment"));

    std::vector<double> envelope_data = { 0.0, 0.5, 1.0, 0.8, 0.2, 0.0 };
    segment.set_processing_metadata("envelope", envelope_data);

    auto gain = segment.get_processing_metadata<double>("gain");
    EXPECT_TRUE(gain.has_value());
    EXPECT_DOUBLE_EQ(*gain, 0.8);

    auto frequency = segment.get_processing_metadata<double>("frequency");
    EXPECT_TRUE(frequency.has_value());
    EXPECT_DOUBLE_EQ(*frequency, 440.0);

    auto active = segment.get_processing_metadata<bool>("active");
    EXPECT_TRUE(active.has_value());
    EXPECT_TRUE(*active);

    auto name = segment.get_processing_metadata<std::string>("name");
    EXPECT_TRUE(name.has_value());
    EXPECT_EQ(*name, "test_segment");

    auto envelope = segment.get_processing_metadata<std::vector<double>>("envelope");
    EXPECT_TRUE(envelope.has_value());
    EXPECT_EQ(envelope->size(), 6);
    EXPECT_DOUBLE_EQ((*envelope)[2], 1.0);

    auto missing = segment.get_processing_metadata<int>("missing");
    EXPECT_FALSE(missing.has_value());

    auto wrong_type = segment.get_processing_metadata<int>("gain");
    EXPECT_FALSE(wrong_type.has_value());

    segment.set_processing_metadata("gain", 1.2);
    auto new_gain = segment.get_processing_metadata<double>("gain");
    EXPECT_TRUE(new_gain.has_value());
    EXPECT_DOUBLE_EQ(*new_gain, 1.2);
}

class RegionGroupTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        region.push_back(Region::time_point(100, "onset"));
        region.push_back(Region::time_span(200, 300, "sustain"));
        region.push_back(Region::time_point(400, "release"));

        group = RegionGroup("test_group", region);
        group.set_attribute("tempo", 120.0);
        group.set_attribute("key", std::string("C_major"));
    }

    std::vector<Region> region;
    RegionGroup group;
};

TEST_F(RegionGroupTest, BasicConstruction)
{
    RegionGroup empty_group;
    EXPECT_TRUE(empty_group.name.empty());
    EXPECT_TRUE(empty_group.regions.empty());
    EXPECT_EQ(empty_group.current_region_index, 0);
    EXPECT_EQ(empty_group.state, RegionState::IDLE);
    EXPECT_EQ(empty_group.transition_type, RegionTransition::IMMEDIATE);
    EXPECT_EQ(empty_group.region_selection_pattern, RegionSelectionPattern::SEQUENTIAL);

    EXPECT_EQ(group.name, "test_group");
    EXPECT_EQ(group.regions.size(), 3);
    EXPECT_EQ(group.regions[0].get_label(), "onset");
    EXPECT_EQ(group.regions[1].get_label(), "sustain");
    EXPECT_EQ(group.regions[2].get_label(), "release");

    std::unordered_map<std::string, std::any> attrs;
    attrs["volume"] = 0.8;
    attrs["category"] = std::string("percussion");

    RegionGroup attr_group("attr_group", {}, attrs);
    auto volume = attr_group.get_attribute<double>("volume");
    EXPECT_TRUE(volume.has_value());
    EXPECT_DOUBLE_EQ(*volume, 0.8);

    auto category = attr_group.get_attribute<std::string>("category");
    EXPECT_TRUE(category.has_value());
    EXPECT_EQ(*category, "percussion");
}

TEST_F(RegionGroupTest, RegionManagement)
{
    auto new_point = Region::time_point(500, "fade_out");
    group.add_region(new_point);
    EXPECT_EQ(group.regions.size(), 4);
    EXPECT_EQ(group.regions[3].get_label(), "fade_out");

    auto insert_point = Region::time_point(150, "attack");
    group.insert_region(1, insert_point);
    EXPECT_EQ(group.regions.size(), 5);
    EXPECT_EQ(group.regions[1].get_label(), "attack");
    EXPECT_EQ(group.regions[2].get_label(), "sustain"); // shifted

    auto end_point = Region::time_point(600, "end");
    group.insert_region(group.regions.size(), end_point);
    EXPECT_EQ(group.regions.size(), 6);
    EXPECT_EQ(group.regions[5].get_label(), "end");

    // Fix: Check what happens when inserting at an invalid index
    auto invalid_insert = Region::time_point(700, "invalid");
    group.insert_region(100, invalid_insert);
    EXPECT_EQ(group.regions.size(), 7);
    EXPECT_EQ(group.regions[group.regions.size() - 1].get_label(), "invalid");

    group.remove_region(1);
    EXPECT_EQ(group.regions.size(), 6);
    EXPECT_EQ(group.regions[1].get_label(), "sustain"); // back to original

    group.current_region_index = 5;
    group.remove_region(5);
    EXPECT_EQ(group.regions.size(), 5);
    EXPECT_EQ(group.current_region_index, 4); // adjusted to valid index

    size_t original_size = group.regions.size();
    group.remove_region(100);
    EXPECT_EQ(group.regions.size(), original_size);

    group.clear_regions();
    EXPECT_TRUE(group.regions.empty());
    EXPECT_EQ(group.current_region_index, 0);
    EXPECT_TRUE(group.active_indices.empty());
}

TEST_F(RegionGroupTest, AttributeManagement)
{
    auto tempo = group.get_attribute<double>("tempo");
    EXPECT_TRUE(tempo.has_value());
    EXPECT_DOUBLE_EQ(*tempo, 120.0);

    auto key = group.get_attribute<std::string>("key");
    EXPECT_TRUE(key.has_value());
    EXPECT_EQ(*key, "C_major");

    group.set_attribute("volume", 0.8);
    group.set_attribute("looping", true);

    auto volume = group.get_attribute<double>("volume");
    EXPECT_TRUE(volume.has_value());
    EXPECT_DOUBLE_EQ(*volume, 0.8);

    auto looping = group.get_attribute<bool>("looping");
    EXPECT_TRUE(looping.has_value());
    EXPECT_TRUE(*looping);

    std::vector<std::string> tags = { "melodic", "percussive", "ambient" };
    group.set_attribute("tags", tags);

    auto retrieved_tags = group.get_attribute<std::vector<std::string>>("tags");
    EXPECT_TRUE(retrieved_tags.has_value());
    EXPECT_EQ(retrieved_tags->size(), 3);
    EXPECT_EQ((*retrieved_tags)[1], "percussive");

    auto missing = group.get_attribute<int>("missing");
    EXPECT_FALSE(missing.has_value());

    auto wrong_type = group.get_attribute<int>("tempo");
    EXPECT_FALSE(wrong_type.has_value());

    group.set_attribute("tempo", 140.0);
    auto new_tempo = group.get_attribute<double>("tempo");
    EXPECT_TRUE(new_tempo.has_value());
    EXPECT_DOUBLE_EQ(*new_tempo, 140.0);
}

TEST_F(RegionGroupTest, SortingOperations)
{
    group.clear_regions();
    group.add_region(Region::time_point(300, "third"));
    group.add_region(Region::time_point(100, "first"));
    group.add_region(Region::time_point(200, "second"));

    group.sort_by_dimension(0);
    EXPECT_EQ(group.regions[0].start_coordinates[0], 100);
    EXPECT_EQ(group.regions[1].start_coordinates[0], 200);
    EXPECT_EQ(group.regions[2].start_coordinates[0], 300);
    EXPECT_EQ(group.regions[0].get_label(), "first");
    EXPECT_EQ(group.regions[1].get_label(), "second");
    EXPECT_EQ(group.regions[2].get_label(), "third");

    group.regions[0].set_attribute("priority", 3.0);
    group.regions[1].set_attribute("priority", 1.0);
    group.regions[2].set_attribute("priority", 2.0);

    group.sort_by_attribute("priority");
    EXPECT_EQ(group.regions[0].get_label(), "second"); // priority 1.0
    EXPECT_EQ(group.regions[1].get_label(), "third"); // priority 2.0
    EXPECT_EQ(group.regions[2].get_label(), "first"); // priority 3.0

    group.regions[0].set_attribute("energy", 0.5);
    // points[1] and points[2] don't have "energy" attribute
    group.sort_by_attribute("energy");

    RegionGroup empty_group;
    empty_group.sort_by_dimension(0);
    empty_group.sort_by_attribute("nonexistent");

    group.sort_by_dimension(10);
}

TEST_F(RegionGroupTest, SearchOperations)
{
    auto onset_points = group.find_regions_with_label("onset");
    EXPECT_EQ(onset_points.size(), 1);
    EXPECT_EQ(onset_points[0].start_coordinates[0], 100);

    auto nonexistent = group.find_regions_with_label("nonexistent");
    EXPECT_TRUE(nonexistent.empty());

    group.add_region(Region::time_point(500, "onset"));
    auto multiple_onsets = group.find_regions_with_label("onset");
    EXPECT_EQ(multiple_onsets.size(), 2);

    group.regions[0].set_attribute("type", std::string("percussive"));
    group.regions[1].set_attribute("type", std::string("tonal"));
    group.regions[2].set_attribute("type", std::string("percussive"));
    group.regions[3].set_attribute("type", std::string("noise"));

    auto percussive_points = group.find_regions_with_attribute("type", std::string("percussive"));
    EXPECT_EQ(percussive_points.size(), 2);
    EXPECT_EQ(percussive_points[0].get_label(), "onset");
    EXPECT_EQ(percussive_points[1].get_label(), "release");

    auto tonal_points = group.find_regions_with_attribute("type", std::string("tonal"));
    EXPECT_EQ(tonal_points.size(), 1);
    EXPECT_EQ(tonal_points[0].get_label(), "sustain");

    group.regions[0].set_attribute("energy", 0.8);
    group.regions[1].set_attribute("energy", 0.8);
    auto high_energy = group.find_regions_with_attribute("energy", 0.8);
    EXPECT_EQ(high_energy.size(), 2);

    auto containing_250 = group.find_regions_containing_coordinates({ 250 });
    EXPECT_EQ(containing_250.size(), 1); // Only the sustain span (200-300) contains 250
    EXPECT_EQ(containing_250[0].get_label(), "sustain");

    auto containing_100 = group.find_regions_containing_coordinates({ 100 });
    EXPECT_EQ(containing_100.size(), 1); // Only the onset point at 100
    EXPECT_EQ(containing_100[0].get_label(), "onset");

    auto containing_none = group.find_regions_containing_coordinates({ 1000 });
    EXPECT_TRUE(containing_none.empty()); // No points contain 1000

    group.add_region(Region::audio_span(600, 700, 0, 1, "multi_dim"));
    auto containing_multi = group.find_regions_containing_coordinates({ 650, 0 });
    EXPECT_EQ(containing_multi.size(), 1);
    EXPECT_EQ(containing_multi[0].get_label(), "multi_dim");

    auto dimension_mismatch = group.find_regions_containing_coordinates({ 250, 0, 0 }); // 3D coords
    EXPECT_TRUE(dimension_mismatch.empty()); // Sustain is 1D, won't match
}

TEST_F(RegionGroupTest, BoundingRegion)
{
    auto bounding = group.get_bounding_region();

    EXPECT_EQ(bounding.start_coordinates[0], 100);
    EXPECT_EQ(bounding.end_coordinates[0], 400);

    auto type = bounding.get_attribute<std::string>("type");
    EXPECT_TRUE(type.has_value());
    if (type.has_value()) {
        EXPECT_EQ(*type, "bounding_box");
    }

    auto source_group = bounding.get_attribute<std::string>("source_group");
    EXPECT_TRUE(source_group.has_value());
    EXPECT_EQ(*source_group, "test_group");

    RegionGroup multi_group("multi_group");
    multi_group.add_region(Region::audio_span(0, 100, 0, 1, "a"));
    multi_group.add_region(Region::audio_span(50, 200, 1, 3, "b"));
    multi_group.add_region(Region::audio_span(25, 150, 2, 2, "c"));

    auto multi_bounding = multi_group.get_bounding_region();
    EXPECT_EQ(multi_bounding.start_coordinates[0], 0);
    EXPECT_EQ(multi_bounding.end_coordinates[0], 200);
    EXPECT_EQ(multi_bounding.start_coordinates[1], 0);
    EXPECT_EQ(multi_bounding.end_coordinates[1], 3);

    RegionGroup empty_group;
    auto empty_bounding = empty_group.get_bounding_region();
    EXPECT_TRUE(empty_bounding.start_coordinates.empty());
    EXPECT_TRUE(empty_bounding.end_coordinates.empty());

    RegionGroup single_group("single");
    single_group.add_region(Region::time_point(500, "single"));
    auto single_bounding = single_group.get_bounding_region();
    EXPECT_EQ(single_bounding.start_coordinates[0], 500);
    EXPECT_EQ(single_bounding.end_coordinates[0], 500);
}

TEST_F(RegionGroupTest, StateAndTransitionManagement)
{

    EXPECT_EQ(group.state, RegionState::IDLE);
    EXPECT_EQ(group.transition_type, RegionTransition::IMMEDIATE);
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::SEQUENTIAL);
    EXPECT_EQ(group.transition_duration_ms, 0.0);

    group.state = RegionState::ACTIVE;
    EXPECT_EQ(group.state, RegionState::ACTIVE);

    group.state = RegionState::TRANSITIONING;
    EXPECT_EQ(group.state, RegionState::TRANSITIONING);

    group.transition_type = RegionTransition::CROSSFADE;
    group.transition_duration_ms = 100.0;
    EXPECT_EQ(group.transition_type, RegionTransition::CROSSFADE);
    EXPECT_DOUBLE_EQ(group.transition_duration_ms, 100.0);

    group.transition_type = RegionTransition::OVERLAP;
    EXPECT_EQ(group.transition_type, RegionTransition::OVERLAP);

    group.transition_type = RegionTransition::GATED;
    EXPECT_EQ(group.transition_type, RegionTransition::GATED);

    group.transition_type = RegionTransition::CALLBACK;
    EXPECT_EQ(group.transition_type, RegionTransition::CALLBACK);

    group.region_selection_pattern = RegionSelectionPattern::RANDOM;
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::RANDOM);

    group.region_selection_pattern = RegionSelectionPattern::ROUND_ROBIN;
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::ROUND_ROBIN);

    group.region_selection_pattern = RegionSelectionPattern::WEIGHTED;
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::WEIGHTED);

    group.region_selection_pattern = RegionSelectionPattern::OVERLAP;
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::OVERLAP);

    group.region_selection_pattern = RegionSelectionPattern::EXCLUSIVE;
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::EXCLUSIVE);

    group.region_selection_pattern = RegionSelectionPattern::CUSTOM;
    EXPECT_EQ(group.region_selection_pattern, RegionSelectionPattern::CUSTOM);
}

TEST_F(RegionGroupTest, ActiveIndicesManagement)
{

    EXPECT_EQ(group.current_region_index, 0);
    EXPECT_TRUE(group.active_indices.empty());

    group.active_indices = { 0, 2 };
    EXPECT_EQ(group.active_indices.size(), 2);
    EXPECT_EQ(group.active_indices[0], 0);
    EXPECT_EQ(group.active_indices[1], 2);

    group.current_region_index = 1;
    EXPECT_EQ(group.current_region_index, 1);

    group.current_region_index = 100;
    EXPECT_EQ(group.current_region_index, 100);

    group.active_indices.clear();
    EXPECT_TRUE(group.active_indices.empty());
}

class RegionCacheTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
        region = Region::time_span(100, 200, "cached_region");

        cache.data = test_data;
        cache.source_region = region;
        cache.load_time = std::chrono::steady_clock::now();
        cache.access_count = 0;
        cache.is_dirty = false;
    }

    DataVariant test_data;
    Region region;
    RegionCache cache;
};

TEST_F(RegionCacheTest, BasicOperations)
{

    EXPECT_FALSE(cache.is_dirty);
    EXPECT_EQ(cache.access_count, 0);
    EXPECT_EQ(cache.source_region, region);

    cache.mark_accessed();
    EXPECT_EQ(cache.access_count, 1);
    cache.mark_accessed();
    EXPECT_EQ(cache.access_count, 2);

    cache.mark_dirty();
    EXPECT_TRUE(cache.is_dirty);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto age = cache.age();
    EXPECT_GT(age.count(), 0.01);
    EXPECT_LT(age.count(), 1.0);

    auto cached_data = std::get<std::vector<double>>(cache.data);
    EXPECT_EQ(cached_data.size(), 5);
    EXPECT_DOUBLE_EQ(cached_data[0], 1.0);
    EXPECT_DOUBLE_EQ(cached_data[4], 5.0);
}

TEST_F(RegionCacheTest, DataVariantHandling)
{

    RegionCache float_cache;
    float_cache.data = std::vector<float> { 1.0f, 2.0f, 3.0f };
    float_cache.source_region = region;

    auto float_data = std::get<std::vector<float>>(float_cache.data);
    EXPECT_EQ(float_data.size(), 3);
    EXPECT_FLOAT_EQ(float_data[1], 2.0f);

    RegionCache complex_cache;
    complex_cache.data = std::vector<std::complex<double>> {
        { 1.0, 0.0 }, { 0.0, 1.0 }, { -1.0, 0.0 }
    };
    complex_cache.source_region = region;

    auto complex_data = std::get<std::vector<std::complex<double>>>(complex_cache.data);
    EXPECT_EQ(complex_data.size(), 3);
    EXPECT_DOUBLE_EQ(complex_data[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(complex_data[1].imag(), 1.0);

    RegionCache int_cache;
    int_cache.data = std::vector<u_int16_t> { 100, 200, 300 };
    int_cache.source_region = region;

    auto int_data = std::get<std::vector<u_int16_t>>(int_cache.data);
    EXPECT_EQ(int_data.size(), 3);
    EXPECT_EQ(int_data[2], 300);
}

TEST_F(RegionCacheTest, PerformanceMetrics)
{

    for (int i = 0; i < 10; ++i) {
        cache.mark_accessed();
    }
    EXPECT_EQ(cache.access_count, 10);

    auto initial_time = cache.load_time;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(cache.load_time, initial_time);

    auto age1 = cache.age();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto age2 = cache.age();
    EXPECT_GT(age2.count(), age1.count());

    EXPECT_FALSE(cache.is_dirty);
    cache.mark_dirty();
    EXPECT_TRUE(cache.is_dirty);

    cache.is_dirty = false;
    cache.load_time = std::chrono::steady_clock::now();
    EXPECT_FALSE(cache.is_dirty);
}

class RegionUtilityTest : public ::testing::Test {
protected:
    void SetUp() override
    {

        region1 = Region::time_span(100, 200, "region1");
        region2 = Region::time_span(150, 250, "region2");
        region3 = Region::time_span(300, 400, "region3");

        reg1 = Region::time_point(125, "point1");
        reg2 = Region::time_point(350, "point2");

        test_group = RegionGroup("utility_test");
        test_group.add_region(region1);
        test_group.add_region(region2);
        test_group.add_region(region3);
        test_group.add_region(reg1);
        test_group.add_region(reg2);
    }

    Region region1, region2, region3;
    Region reg1, reg2;
    RegionGroup test_group;
};

TEST_F(RegionUtilityTest, RegionOverlapDetection)
{

    EXPECT_TRUE(region1.overlaps(region2)); // 100-200 overlaps 150-250
    EXPECT_FALSE(region1.overlaps(region3)); // 100-200 doesn't overlap 300-400
    EXPECT_FALSE(region1.overlaps(region3)); // 150-250 doesn't overlap 300-400

    EXPECT_TRUE(reg1.overlaps(region1)); // point at 125 overlaps 100-200
    EXPECT_FALSE(reg2.overlaps(region1)); // point at 350 doesn't overlap 100-200

    EXPECT_TRUE(region1.overlaps(region1));
    EXPECT_TRUE(reg1.overlaps(reg1));
}

TEST_F(RegionUtilityTest, RegionContainment)
{

    EXPECT_TRUE(region1.contains({ 150 })); // 150 is in 100-200
    EXPECT_FALSE(region1.contains({ 250 })); // 250 is not in 100-200

    EXPECT_TRUE(region1.contains({ 100 })); // start boundary
    EXPECT_TRUE(region1.contains({ 200 })); // end boundary
    EXPECT_FALSE(region1.contains({ 99 })); // just before
    EXPECT_FALSE(region1.contains({ 201 })); // just after

    auto multi_region = Region::audio_span(100, 200, 0, 2, "multi");
    EXPECT_TRUE(multi_region.contains({ 150, 1 }));
    EXPECT_FALSE(multi_region.contains({ 150, 3 }));
    EXPECT_FALSE(multi_region.contains({ 250, 1 }));
}

TEST_F(RegionUtilityTest, RegionTransformations)
{

    auto translated = translate_region(region1, { 50 });
    EXPECT_EQ(translated.start_coordinates[0], 150); // 100 + 50
    EXPECT_EQ(translated.end_coordinates[0], 250); // 200 + 50

    auto neg_translated = translate_region(region1, { -50 });
    EXPECT_EQ(neg_translated.start_coordinates[0], 50); // 100 - 50
    EXPECT_EQ(neg_translated.end_coordinates[0], 150); // 200 - 50

    auto multi_region = Region::audio_span(100, 200, 1, 2, "multi");
    auto multi_translated = translate_region(multi_region, { 10, -1 });
    EXPECT_EQ(multi_translated.start_coordinates[0], 110);
    EXPECT_EQ(multi_translated.start_coordinates[1], 0);
    EXPECT_EQ(multi_translated.end_coordinates[0], 210);
    EXPECT_EQ(multi_translated.end_coordinates[1], 1);

    auto scaled = scale_region(region1, { 2.0 });
    u_int64_t center = (100 + 200) / 2; // 150
    u_int64_t half_span = (200 - 100) / 2; // 50
    u_int64_t new_half_span = static_cast<u_int64_t>(half_span * 2.0); // 100
    EXPECT_EQ(scaled.start_coordinates[0], center - new_half_span); // 50
    EXPECT_EQ(scaled.end_coordinates[0], center + new_half_span); // 250

    auto shrunk = scale_region(region1, { 0.5 });
    u_int64_t shrunk_half_span = static_cast<u_int64_t>(half_span * 0.5); // 25
    EXPECT_EQ(shrunk.start_coordinates[0], center - shrunk_half_span); // 125
    EXPECT_EQ(shrunk.end_coordinates[0], center + shrunk_half_span); // 175
}

TEST_F(RegionUtilityTest, GroupBoundingRegion)
{
    auto bounding = get_bounding_region(test_group);

    // Should encompass all points from 100 to 400
    EXPECT_EQ(bounding.start_coordinates[0], 100);
    EXPECT_EQ(bounding.end_coordinates[0], 400);

    RegionGroup empty_group("empty");
    auto empty_bounding = get_bounding_region(empty_group);
    EXPECT_TRUE(empty_bounding.start_coordinates.empty());

    RegionGroup single_group("single");
    single_group.add_region(reg1);
    auto single_bounding = get_bounding_region(single_group);
    EXPECT_EQ(single_bounding.start_coordinates[0], 125);
    EXPECT_EQ(single_bounding.end_coordinates[0], 125);
}

TEST_F(RegionUtilityTest, PointSorting)
{
    std::vector<Region> points = { region3, region1, region2, reg2, reg1 };

    sort_regions_by_dimension(points, 0);
    EXPECT_EQ(points[0].start_coordinates[0], 100); // region1
    EXPECT_EQ(points[1].start_coordinates[0], 125); // point1
    EXPECT_EQ(points[2].start_coordinates[0], 150); // region2
    EXPECT_EQ(points[3].start_coordinates[0], 300); // region3
    EXPECT_EQ(points[4].start_coordinates[0], 350); // point2

    for (auto& point : points) {
        if (point.get_label() == "region1")
            point.set_attribute("priority", std::string("high"));
        else if (point.get_label() == "region2")
            point.set_attribute("priority", std::string("low"));
        else if (point.get_label() == "region3")
            point.set_attribute("priority", std::string("medium"));
        else
            point.set_attribute("priority", std::string("high"));
    }

    sort_regions_by_attribute(points, "priority");
    // Should group by priority: high, low, medium (alphabetical)
    // Implementation may vary, but should handle string sorting
}

TEST_F(RegionUtilityTest, AttributeUtilities)
{
    Region test_point = Region::time_point(100, "test");

    set_region_attribute(test_point, "energy", 0.75);
    set_region_attribute(test_point, "frequency", 440.0);
    set_region_attribute(test_point, "note", std::string("A4"));

    auto energy = get_region_attribute<double>(test_point, "energy");
    EXPECT_TRUE(energy.has_value());
    EXPECT_DOUBLE_EQ(*energy, 0.75);

    auto frequency = get_region_attribute<double>(test_point, "frequency");
    EXPECT_TRUE(frequency.has_value());
    EXPECT_DOUBLE_EQ(*frequency, 440.0);

    auto note = get_region_attribute<std::string>(test_point, "note");
    EXPECT_TRUE(note.has_value());
    EXPECT_EQ(*note, "A4");

    test_point.set_label("test_label");
    EXPECT_EQ(test_point.get_label(), "test_label");

    test_group.regions[0].set_attribute("category", std::string("onset"));
    test_group.regions[1].set_attribute("category", std::string("sustain"));
    test_group.regions[2].set_attribute("category", std::string("release"));

    auto onset_points = find_regions_with_label(test_group, "region1");
    EXPECT_EQ(onset_points.size(), 1);

    auto category_points = find_regions_with_attribute(test_group, "category", std::string("onset"));
    EXPECT_EQ(category_points.size(), 1);

    auto containing_points = find_regions_containing_coordinates(test_group, { 175 });
    EXPECT_EQ(containing_points.size(), 2);
}

class DSPRegionTest : public ::testing::Test {
protected:
    void SetUp() override
    {

        onset_detection = Region::time_point(1000, "onset");
        onset_detection.set_attribute("energy", 0.85);
        onset_detection.set_attribute("spectral_flux", 0.72);
        onset_detection.set_attribute("algorithm", std::string("complex_domain"));

        formant_region = Region::audio_span(500, 1500, 800, 1200, "formant_f1");
        formant_region.set_attribute("center_frequency", 950.0);
        formant_region.set_attribute("bandwidth", 100.0);
        formant_region.set_attribute("formant_number", 1);

        zero_crossing_cluster = Region::time_span(2000, 2100, "zc_cluster");
        zero_crossing_cluster.set_attribute("crossing_rate", 25.5);
        zero_crossing_cluster.set_attribute("rms_level", -18.0);
        zero_crossing_cluster.set_attribute("spectral_centroid", 3500.0);

        transient_segment = RegionSegment(Region::time_span(0, 512, "transient"));
        transient_segment.set_processing_metadata("attack_time", 0.003);
        transient_segment.set_processing_metadata("decay_coefficient", 0.95);
        transient_segment.set_processing_metadata("filter_cutoff", 8000.0);
    }

    Region onset_detection;
    Region formant_region;
    Region zero_crossing_cluster;
    RegionSegment transient_segment;
};

TEST_F(DSPRegionTest, OnsetDetectionAnalysis)
{

    EXPECT_EQ(onset_detection.get_label(), "onset");
    EXPECT_EQ(onset_detection.start_coordinates[0], 1000);

    auto energy = onset_detection.get_attribute<double>("energy");
    EXPECT_TRUE(energy.has_value());
    EXPECT_DOUBLE_EQ(*energy, 0.85);

    auto flux = onset_detection.get_attribute<double>("spectral_flux");
    EXPECT_TRUE(flux.has_value());
    EXPECT_DOUBLE_EQ(*flux, 0.72);

    RegionGroup onset_group("onset_analysis");
    onset_group.add_region(onset_detection);

    auto onset2 = Region::time_point(1500, "onset");
    onset2.set_attribute("energy", 0.92);
    onset2.set_attribute("spectral_flux", 0.81);
    onset2.set_attribute("algorithm", std::string("complex_domain"));
    onset_group.add_region(onset2);

    auto onset3 = Region::time_point(2200, "onset");
    onset3.set_attribute("energy", 0.78);
    onset3.set_attribute("spectral_flux", 0.65);
    onset3.set_attribute("algorithm", std::string("phase_deviation"));
    onset_group.add_region(onset3);

    std::vector<Region> high_energy_onsets;
    for (const auto& point : onset_group.regions) {
        auto energy_val = point.get_attribute<double>("energy");
        if (energy_val && *energy_val > 0.8) {
            high_energy_onsets.push_back(point);
        }
    }

    EXPECT_EQ(high_energy_onsets.size(), 2); // onset1 and onset2

    std::vector<double> ioi_values;
    if (onset_group.regions.size() > 1) {
        sort_regions_by_dimension(onset_group.regions, 0);
        for (size_t i = 1; i < onset_group.regions.size(); i++) {
            double ioi = onset_group.regions[i].start_coordinates[0] - onset_group.regions[i - 1].start_coordinates[0];
            ioi_values.push_back(ioi);
        }
    }

    EXPECT_EQ(ioi_values.size(), 2);
    EXPECT_DOUBLE_EQ(ioi_values[0], 500); // 1500 - 1000
    EXPECT_DOUBLE_EQ(ioi_values[1], 700); // 2200 - 1500
}

TEST_F(DSPRegionTest, SpectralRegionAnalysis)
{

    EXPECT_EQ(formant_region.get_label(), "formant_f1");
    EXPECT_EQ(formant_region.start_coordinates[0], 500); // time start
    EXPECT_EQ(formant_region.end_coordinates[0], 1500); // time end
    EXPECT_EQ(formant_region.start_coordinates[1], 800); // freq start
    EXPECT_EQ(formant_region.end_coordinates[1], 1200); // freq end

    auto center_freq = formant_region.get_attribute<double>("center_frequency");
    EXPECT_TRUE(center_freq.has_value());
    EXPECT_DOUBLE_EQ(*center_freq, 950.0);

    RegionGroup formant_group("formant_tracking");
    formant_group.add_region(formant_region);

    auto formant2 = Region::audio_span(500, 1500, 1800, 2200, "formant_f2");
    formant2.set_attribute("center_frequency", 2000.0);
    formant2.set_attribute("bandwidth", 150.0);
    formant2.set_attribute("formant_number", 2);
    formant_group.add_region(formant2);

    auto formant3 = Region::audio_span(500, 1500, 2700, 3300, "formant_f3");
    formant3.set_attribute("center_frequency", 3000.0);
    formant3.set_attribute("bandwidth", 200.0);
    formant3.set_attribute("formant_number", 3);
    formant_group.add_region(formant3);

    std::vector<double> formant_ratios;
    if (formant_group.regions.size() >= 3) {
        auto f1 = formant_group.regions[0].get_attribute<double>("center_frequency");
        auto f2 = formant_group.regions[1].get_attribute<double>("center_frequency");
        auto f3 = formant_group.regions[2].get_attribute<double>("center_frequency");

        if (f1 && f2 && f3) {
            formant_ratios.push_back(*f2 / *f1);
            formant_ratios.push_back(*f3 / *f1);
        }
    }

    EXPECT_EQ(formant_ratios.size(), 2);
    EXPECT_DOUBLE_EQ(formant_ratios[0], 2000.0 / 950.0);
    EXPECT_DOUBLE_EQ(formant_ratios[1], 3000.0 / 950.0);

    double weighted_sum = 0.0;
    double total_weight = 0.0;

    for (const auto& formant : formant_group.regions) {
        auto center = formant.get_attribute<double>("center_frequency");
        auto bandwidth = formant.get_attribute<double>("bandwidth");

        if (center && bandwidth) {
            weighted_sum += *center * *bandwidth;
            total_weight += *bandwidth;
        }
    }

    double spectral_centroid = weighted_sum / total_weight;
    EXPECT_GT(spectral_centroid, 0.0);

    auto f2_formants = find_regions_containing_coordinates(formant_group, { 1000, 2000 });
    EXPECT_EQ(f2_formants.size(), 1);
    EXPECT_EQ(f2_formants[0].get_label(), "formant_f2");
}

TEST_F(DSPRegionTest, TransientProcessing)
{

    EXPECT_EQ(transient_segment.source_region.get_label(), "transient");
    EXPECT_EQ(transient_segment.source_region.start_coordinates[0], 0);
    EXPECT_EQ(transient_segment.source_region.end_coordinates[0], 512);

    auto attack_time = transient_segment.get_processing_metadata<double>("attack_time");
    EXPECT_TRUE(attack_time.has_value());
    EXPECT_DOUBLE_EQ(*attack_time, 0.003);

    auto decay_coef = transient_segment.get_processing_metadata<double>("decay_coefficient");
    EXPECT_TRUE(decay_coef.has_value());
    EXPECT_DOUBLE_EQ(*decay_coef, 0.95);

    std::vector<RegionSegment> drum_hits;

    auto kick = RegionSegment(Region::time_span(1000, 1512, "kick"));
    kick.set_processing_metadata("attack_time", 0.005);
    kick.set_processing_metadata("decay_coefficient", 0.98);
    kick.set_processing_metadata("peak_frequency", 80.0);
    kick.set_processing_metadata("instrument", std::string("kick_drum"));
    drum_hits.push_back(kick);

    auto snare = RegionSegment(Region::time_span(2000, 2512, "snare"));
    snare.set_processing_metadata("attack_time", 0.002);
    snare.set_processing_metadata("decay_coefficient", 0.92);
    snare.set_processing_metadata("peak_frequency", 240.0);
    snare.set_processing_metadata("instrument", std::string("snare_drum"));
    drum_hits.push_back(snare);

    auto hihat = RegionSegment(Region::time_span(3000, 3256, "hihat"));
    hihat.set_processing_metadata("attack_time", 0.001);
    hihat.set_processing_metadata("decay_coefficient", 0.85);
    hihat.set_processing_metadata("peak_frequency", 8000.0);
    hihat.set_processing_metadata("instrument", std::string("hi_hat"));
    drum_hits.push_back(hihat);

    std::vector<std::string> fast_attacks;
    std::vector<std::string> high_frequency;

    for (const auto& hit : drum_hits) {
        auto attack = hit.get_processing_metadata<double>("attack_time");
        auto freq = hit.get_processing_metadata<double>("peak_frequency");
        auto instrument = hit.get_processing_metadata<std::string>("instrument");

        if (attack && *attack <= 0.002 && instrument) {
            fast_attacks.push_back(*instrument);
        }

        if (freq && *freq > 5000.0 && instrument) {
            high_frequency.push_back(*instrument);
        }
    }

    EXPECT_EQ(fast_attacks.size(), 2);
    // EXPECT_EQ(fast_attacks[0], "snare_drum");
    // EXPECT_EQ(fast_attacks[1], "hi_hat");

    EXPECT_EQ(high_frequency.size(), 1);
    // EXPECT_EQ(high_frequency[0], "hi_hat");

    // Test calculating average decay coefficient
    double total_decay = 0.0;
    for (const auto& hit : drum_hits) {
        auto decay = hit.get_processing_metadata<double>("decay_coefficient");
        if (decay) {
            total_decay += *decay;
        }
    }
    double avg_decay = total_decay / drum_hits.size();

    EXPECT_GT(avg_decay, 0.0);
    EXPECT_LT(avg_decay, 1.0);
}

TEST_F(DSPRegionTest, ZeroCrossingAnalysis)
{

    EXPECT_EQ(zero_crossing_cluster.get_label(), "zc_cluster");
    EXPECT_EQ(zero_crossing_cluster.start_coordinates[0], 2000);
    EXPECT_EQ(zero_crossing_cluster.end_coordinates[0], 2100);

    auto crossing_rate = zero_crossing_cluster.get_attribute<double>("crossing_rate");
    EXPECT_TRUE(crossing_rate.has_value());
    EXPECT_DOUBLE_EQ(*crossing_rate, 25.5);

    auto rms_level = zero_crossing_cluster.get_attribute<double>("rms_level");
    EXPECT_TRUE(rms_level.has_value());
    EXPECT_DOUBLE_EQ(*rms_level, -18.0);

    RegionGroup zc_group("zero_crossing_analysis");
    zc_group.add_region(zero_crossing_cluster);

    auto zc2 = Region::time_span(2200, 2300, "zc_cluster");
    zc2.set_attribute("crossing_rate", 35.2);
    zc2.set_attribute("rms_level", -15.0);
    zc2.set_attribute("spectral_centroid", 4200.0);
    zc_group.add_region(zc2);

    auto zc3 = Region::time_span(2400, 2500, "zc_cluster");
    zc3.set_attribute("crossing_rate", 18.7);
    zc3.set_attribute("rms_level", -22.0);
    zc3.set_attribute("spectral_centroid", 2800.0);
    zc_group.add_region(zc3);

    std::vector<Region> noise_regions;
    std::vector<Region> tonal_regions;

    for (auto& region : zc_group.regions) {
        auto zcr = region.get_attribute<double>("crossing_rate");
        if (zcr) {
            if (*zcr > 30.0) {
                noise_regions.push_back(region);
                region.set_attribute("classification", std::string("noise"));
            } else {
                tonal_regions.push_back(region);
                region.set_attribute("classification", std::string("tonal"));
            }
        }
    }

    EXPECT_EQ(noise_regions.size(), 1);
    EXPECT_EQ(tonal_regions.size(), 2);

    bool correlation_positive = true;
    for (size_t i = 1; i < zc_group.regions.size(); i++) {
        auto zcr1 = zc_group.regions[i - 1].get_attribute<double>("crossing_rate");
        auto zcr2 = zc_group.regions[i].get_attribute<double>("crossing_rate");
        auto sc1 = zc_group.regions[i - 1].get_attribute<double>("spectral_centroid");
        auto sc2 = zc_group.regions[i].get_attribute<double>("spectral_centroid");

        if (zcr1 && zcr2 && sc1 && sc2) {

            if ((*zcr2 > *zcr1 && *sc2 < *sc1) || (*zcr2 < *zcr1 && *sc2 > *sc1)) {
                correlation_positive = false;
                break;
            }
        }
    }

    EXPECT_TRUE(correlation_positive);
}
}
