#include <algorithm>

#include "../test_config.h"

#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Filters/IIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Network/ModalNetwork.hpp"
#include "MayaFlux/Nodes/Network/WaveguideNetwork.hpp"

#include "MayaFlux/API/Core.hpp"
#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {

//-----------------------------------------------------------------------------
// Unit Tests - No engine required
//-----------------------------------------------------------------------------

class ModalNetworkUnitTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Config::set_sample_rate(TestConfig::SAMPLE_RATE);
    }
};

TEST_F(ModalNetworkUnitTest, SpectrumGenerationAndModeProperties)
{
    auto harmonic = std::make_shared<Nodes::Network::ModalNetwork>(
        4, 100.0, Nodes::Network::ModalNetwork::Spectrum::HARMONIC);

    EXPECT_EQ(harmonic->get_node_count(), 4);
    EXPECT_DOUBLE_EQ(harmonic->get_fundamental(), 100.0);

    const auto& h_modes = harmonic->get_modes();
    EXPECT_DOUBLE_EQ(h_modes[0].frequency_ratio, 1.0);
    EXPECT_DOUBLE_EQ(h_modes[1].frequency_ratio, 2.0);
    EXPECT_DOUBLE_EQ(h_modes[2].frequency_ratio, 3.0);
    EXPECT_DOUBLE_EQ(h_modes[3].frequency_ratio, 4.0);

    auto inharmonic = std::make_shared<Nodes::Network::ModalNetwork>(
        3, 100.0, Nodes::Network::ModalNetwork::Spectrum::INHARMONIC);

    const auto& i_modes = inharmonic->get_modes();
    EXPECT_DOUBLE_EQ(i_modes[0].frequency_ratio, 1.0);
    EXPECT_NEAR(i_modes[1].frequency_ratio, 2.756, 0.01);
    EXPECT_NEAR(i_modes[2].frequency_ratio, 5.404, 0.01);

    std::vector<double> custom_ratios = { 1.0, 1.5, 2.25 };
    auto custom = std::make_shared<Nodes::Network::ModalNetwork>(custom_ratios, 200.0);

    const auto& c_modes = custom->get_modes();
    for (size_t i = 0; i < custom_ratios.size(); ++i) {
        EXPECT_DOUBLE_EQ(c_modes[i].frequency_ratio, custom_ratios[i]);
        EXPECT_DOUBLE_EQ(c_modes[i].base_frequency, 200.0 * custom_ratios[i]);
        EXPECT_NE(c_modes[i].oscillator, nullptr);
    }

    for (size_t i = 0; i < h_modes.size(); ++i) {
        EXPECT_DOUBLE_EQ(h_modes[i].initial_amplitude, 1.0 / (i + 1));
        EXPECT_EQ(h_modes[i].amplitude, 0.0);
    }
}

TEST_F(ModalNetworkUnitTest, ExcitationAndDamping)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(6, 220.0);

    bell->excite(1.0);
    const auto& modes = bell->get_modes();
    for (const auto& mode : modes) {
        EXPECT_DOUBLE_EQ(mode.amplitude, mode.initial_amplitude);
    }

    bell->excite(0.5);
    for (const auto& mode : modes) {
        EXPECT_NEAR(mode.amplitude, mode.initial_amplitude * 0.5, 1e-6);
    }

    bell->excite(0.0);
    bell->excite_mode(2, 1.0);
    EXPECT_GT(modes[2].amplitude, 0.0);
    for (size_t i = 0; i < modes.size(); ++i) {
        if (i != 2) {
            EXPECT_EQ(modes[i].amplitude, 0.0);
        }
    }

    bell->excite(1.0);
    double initial_sum = 0.0;
    for (const auto& mode : modes) {
        initial_sum += mode.amplitude;
    }

    bell->damp(0.3);
    double damped_sum = 0.0;
    for (const auto& mode : modes) {
        damped_sum += mode.amplitude;
    }
    EXPECT_NEAR(damped_sum, initial_sum * 0.3, 1e-6);
}

TEST_F(ModalNetworkUnitTest, ExciterSystemConfiguration)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(4, 220.0);

    EXPECT_EQ(bell->get_exciter_type(),
        Nodes::Network::ModalNetwork::ExciterType::IMPULSE);

    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::NOISE_BURST);
    bell->set_exciter_duration(0.015);
    EXPECT_EQ(bell->get_exciter_type(),
        Nodes::Network::ModalNetwork::ExciterType::NOISE_BURST);

    auto fir_filter = std::make_shared<Nodes::Filters::FIR>(
        std::vector<double> { 0.25, 0.5, 0.25 });
    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::FILTERED_NOISE);
    bell->set_exciter_filter(fir_filter);

    auto iir_filter = std::make_shared<Nodes::Filters::IIR>(
        std::vector<double> { 1.0, -0.9 },
        std::vector<double> { 0.1 });
    bell->set_exciter_filter(iir_filter);

    std::vector<double> custom_sample(32, 0.5);
    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::SAMPLE);
    bell->set_exciter_sample(custom_sample);

    auto sine_exciter = std::make_shared<Nodes::Generator::Sine>(5.0, 0.2);
    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::CONTINUOUS);
    bell->set_exciter_node(sine_exciter);
}

TEST_F(ModalNetworkUnitTest, SpatialExcitation)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(8, 220.0);

    bell->excite_at_position(0.5, 1.0);
    const auto& modes_center = bell->get_modes();
    for (const auto& mode : modes_center) {
        EXPECT_GT(mode.amplitude, 0.0);
    }

    double position = 0.25;
    bell->excite_at_position(position, 1.0);
    const auto& modes_quarter = bell->get_modes();
    for (size_t i = 0; i < modes_quarter.size(); ++i) {
        double expected_spatial_factor = std::abs(std::sin(((double)i + 1) * M_PI * position));
        double expected_amplitude = modes_quarter[i].initial_amplitude * expected_spatial_factor;
        EXPECT_NEAR(modes_quarter[i].amplitude, expected_amplitude, 0.01);
    }

    std::vector<double> custom_dist = { 1.0, 0.8, 0.6, 0.4, 0.2, 0.1, 0.05, 0.025 };
    bell->set_spatial_distribution(custom_dist);
    const auto& dist = bell->get_spatial_distribution();
    EXPECT_EQ(dist.size(), 8);
    for (size_t i = 0; i < custom_dist.size(); ++i) {
        EXPECT_DOUBLE_EQ(dist[i], custom_dist[i]);
    }

    std::vector<double> wrong_size = { 1.0, 0.5 };
    bell->set_spatial_distribution(wrong_size);
    EXPECT_EQ(bell->get_spatial_distribution().size(), 8);
}

TEST_F(ModalNetworkUnitTest, ModalCoupling)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(8, 220.0);

    bell->set_mode_coupling(0, 3, 0.2);
    bell->set_coupling_enabled(true);
    EXPECT_TRUE(bell->is_coupling_enabled());

    const auto& couplings = bell->get_couplings();
    EXPECT_EQ(couplings.size(), 1);
    EXPECT_EQ(couplings[0].mode_a, 0);
    EXPECT_EQ(couplings[0].mode_b, 3);
    EXPECT_DOUBLE_EQ(couplings[0].strength, 0.2);

    bell->set_mode_coupling(1, 4, 0.15);
    bell->set_mode_coupling(2, 5, 0.10);
    EXPECT_EQ(bell->get_couplings().size(), 3);

    bell->remove_mode_coupling(1, 4);
    EXPECT_EQ(bell->get_couplings().size(), 2);

    bell->clear_couplings();
    EXPECT_EQ(bell->get_couplings().size(), 0);

    bell->set_mode_coupling(0, 1, 1.5);
    EXPECT_DOUBLE_EQ(bell->get_couplings()[0].strength, 1.0);

    bell->set_mode_coupling(2, 3, -0.2);
    EXPECT_DOUBLE_EQ(bell->get_couplings()[1].strength, 0.0);

    size_t initial_count = bell->get_couplings().size();
    bell->set_mode_coupling(5, 5, 0.5);
    EXPECT_EQ(bell->get_couplings().size(), initial_count);

    bell->set_mode_coupling(0, 100, 0.5);
    EXPECT_EQ(bell->get_couplings().size(), initial_count);
}

TEST_F(ModalNetworkUnitTest, MetadataReporting)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(
        6, 440.0, Nodes::Network::ModalNetwork::Spectrum::INHARMONIC);

    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::NOISE_BURST);
    bell->set_mode_coupling(0, 2, 0.3);
    bell->set_coupling_enabled(true);

    auto metadata = bell->get_metadata();

    EXPECT_EQ(metadata["fundamental"], "440.000000 Hz");
    EXPECT_EQ(metadata["spectrum"], "INHARMONIC");
    EXPECT_EQ(metadata["exciter_type"], "NOISE_BURST");
    EXPECT_EQ(metadata["coupling_enabled"], "true");
    EXPECT_EQ(metadata["coupling_count"], "1");
    EXPECT_EQ(metadata["node_count"], "6");
}

//-----------------------------------------------------------------------------
// Integration Tests - Engine required
//-----------------------------------------------------------------------------

class ModalNetworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        MayaFlux::Init();
        MayaFlux::Start();
        node_manager = MayaFlux::get_node_graph_manager();
    }

    void TearDown() override
    {
        MayaFlux::End();
    }

    std::shared_ptr<Nodes::NodeGraphManager> node_manager;
};

TEST_F(ModalNetworkIntegrationTest, ProcessingWithDifferentExciters)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(
        8, 220.0, Nodes::Network::ModalNetwork::Spectrum::INHARMONIC);

    node_manager->add_network(bell, Nodes::ProcessingToken::AUDIO_RATE);

    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::IMPULSE);
    bell->excite(1.0);
    bell->process_batch(128);

    auto impulse_buffer = bell->get_audio_buffer();
    EXPECT_TRUE(impulse_buffer.has_value());
    EXPECT_EQ(impulse_buffer->size(), 128);

    bool has_signal = std::any_of(impulse_buffer->begin(), impulse_buffer->end(),
        [](double s) { return std::abs(s) > 0.001; });
    EXPECT_TRUE(has_signal);

    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::NOISE_BURST);
    bell->set_exciter_duration(0.01);
    bell->excite(0.8);
    bell->process_batch(256);

    auto noise_buffer = bell->get_audio_buffer();
    EXPECT_EQ(noise_buffer->size(), 256);

    auto lowpass = std::make_shared<Nodes::Filters::FIR>(
        std::vector<double> { 0.25, 0.5, 0.25 });
    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::FILTERED_NOISE);
    bell->set_exciter_filter(lowpass);
    bell->excite(0.9);
    bell->process_batch(128);

    auto filtered_buffer = bell->get_audio_buffer();
    EXPECT_EQ(filtered_buffer->size(), 128);
}

TEST_F(ModalNetworkIntegrationTest, SpatialExcitationAndCouplingWithProcessing)
{
    auto bell = std::make_shared<Nodes::Network::ModalNetwork>(
        12, 440.0, Nodes::Network::ModalNetwork::Spectrum::STRETCHED, 2.0);

    node_manager->add_network(bell, Nodes::ProcessingToken::AUDIO_RATE);

    bell->set_mode_coupling(0, 3, 0.25);
    bell->set_mode_coupling(1, 5, 0.15);
    bell->set_mode_coupling(2, 7, 0.10);
    bell->set_coupling_enabled(true);

    bell->excite_at_position(0.5, 1.0);

    const auto& modes_after_center = bell->get_modes();
    bool center_modes_have_amplitude = std::ranges::any_of(modes_after_center,
        [](const auto& m) { return m.amplitude > 0.0001; });
    EXPECT_TRUE(center_modes_have_amplitude);

    bell->process_batch(256);
    auto center_buffer = bell->get_audio_buffer();
    EXPECT_EQ(center_buffer->size(), 256);

    bool center_has_signal = std::any_of(center_buffer->begin(), center_buffer->end(),
        [](double s) { return std::abs(s) > 0.001; });
    EXPECT_TRUE(center_has_signal);

    bell->excite_at_position(0.25, 1.0);

    const auto& modes_after_quarter = bell->get_modes();
    bool quarter_modes_have_amplitude = std::ranges::any_of(modes_after_quarter,
        [](const auto& m) { return m.amplitude > 0.0001; });
    EXPECT_TRUE(quarter_modes_have_amplitude);

    bell->process_batch(256);
    auto quarter_buffer = bell->get_audio_buffer();
    EXPECT_EQ(quarter_buffer->size(), 256);

    bool quarter_has_signal = std::any_of(quarter_buffer->begin(), quarter_buffer->end(),
        [](double s) { return std::abs(s) > 0.001; });
    EXPECT_TRUE(quarter_has_signal);

    auto sine_exciter = std::make_shared<Nodes::Generator::Sine>(8.0, 0.05);
    bell->set_exciter_type(Nodes::Network::ModalNetwork::ExciterType::CONTINUOUS);
    bell->set_exciter_node(sine_exciter);
    bell->excite(1.0);

    bell->process_batch(512);
    auto continuous_buffer = bell->get_audio_buffer();
    EXPECT_EQ(continuous_buffer->size(), 512);

    const auto& modes_continuous = bell->get_modes();
    bool continuous_modes_have_amplitude = std::ranges::any_of(modes_continuous,
        [](const auto& m) { return m.amplitude > 0.0001; });
    EXPECT_TRUE(continuous_modes_have_amplitude);

    bool continuous_has_signal = std::any_of(continuous_buffer->begin(), continuous_buffer->end(),
        [](double s) { return std::abs(s) > 0.001; });
    EXPECT_TRUE(continuous_has_signal);
}

class WaveguideNetworkUnitTest : public ::testing::Test {
};

TEST_F(WaveguideNetworkUnitTest, ConstructionAndDelayGeometry)
{
    Nodes::Network::WaveguideNetwork wg(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 440.0, TestConfig::SAMPLE_RATE);

    EXPECT_DOUBLE_EQ(wg.get_fundamental(), 440.0);
    EXPECT_EQ(wg.get_type(), Nodes::Network::WaveguideNetwork::WaveguideType::STRING);
    EXPECT_EQ(wg.get_node_count(), 1);
    EXPECT_EQ(wg.get_output_mode(), Nodes::Network::OutputMode::AUDIO_SINK);
    EXPECT_EQ(wg.get_topology(), Nodes::Network::Topology::RING);

    const double expected_total = TestConfig::SAMPLE_RATE / 440.0 - 0.5;
    const auto expected_integer = static_cast<size_t>(expected_total);
    ASSERT_EQ(wg.get_segments().size(), 1);
    EXPECT_GE(wg.get_segments()[0].p_plus.capacity(), expected_integer + 2);
    EXPECT_EQ(wg.get_segments()[0].mode,
        Nodes::Network::WaveguideNetwork::WaveguideSegment::PropagationMode::UNIDIRECTIONAL);

    Nodes::Network::WaveguideNetwork wg_low(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 55.0, TestConfig::SAMPLE_RATE);
    const double expected_low = TestConfig::SAMPLE_RATE / 55.0 - 0.5;
    EXPECT_GE(wg_low.get_segments()[0].p_plus.capacity(),
        static_cast<size_t>(expected_low) + 2);
}

TEST_F(WaveguideNetworkUnitTest, TubeConstructionAndBidirectionalGeometry)
{
    Nodes::Network::WaveguideNetwork tube(
        Nodes::Network::WaveguideNetwork::WaveguideType::TUBE, 220.0, TestConfig::SAMPLE_RATE);

    EXPECT_EQ(tube.get_type(), Nodes::Network::WaveguideNetwork::WaveguideType::TUBE);
    ASSERT_EQ(tube.get_segments().size(), 1);

    const auto& seg = tube.get_segments()[0];
    EXPECT_EQ(seg.mode,
        Nodes::Network::WaveguideNetwork::WaveguideSegment::PropagationMode::BIDIRECTIONAL);

    EXPECT_EQ(seg.p_plus.capacity(), seg.p_minus.capacity());
    EXPECT_GE(seg.p_plus.capacity(), 2);

    EXPECT_EQ(seg.loop_filter_closed, nullptr);
    EXPECT_EQ(seg.loop_filter_open, nullptr);
}

TEST_F(WaveguideNetworkUnitTest, MetadataAndFundamentalControl)
{
    Nodes::Network::WaveguideNetwork wg(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);

    auto meta = wg.get_metadata();
    for (const auto& key : { "type", "fundamental", "delay_length", "loss_factor",
             "pickup_position", "exciter_type" }) {
        EXPECT_TRUE(meta.contains(key));
    }
    EXPECT_EQ(meta["type"], "STRING");

    const auto delay_before = meta["delay_length"];
    wg.set_fundamental(440.0);
    EXPECT_DOUBLE_EQ(wg.get_fundamental(), 440.0);
    EXPECT_NE(delay_before, wg.get_metadata()["delay_length"]);

    wg.set_fundamental(5.0);
    EXPECT_DOUBLE_EQ(wg.get_fundamental(), 20.0);
}

TEST_F(WaveguideNetworkUnitTest, PickupPositionClampingAndRoundtrip)
{
    Nodes::Network::WaveguideNetwork wg(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);

    wg.set_pickup_position(-0.5);
    EXPECT_GE(wg.get_pickup_position(), 0.0);

    wg.set_pickup_position(1.5);
    EXPECT_LE(wg.get_pickup_position(), 1.0);

    wg.set_pickup_position(0.1);
    EXPECT_NEAR(wg.get_pickup_position(), 0.1, 0.01);

    const double pos_a = wg.get_pickup_position();
    wg.set_pickup_position(0.9);
    EXPECT_NE(pos_a, wg.get_pickup_position());
}

TEST_F(WaveguideNetworkUnitTest, ExciterTypeConfiguration)
{
    Nodes::Network::WaveguideNetwork wg(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);

    EXPECT_EQ(wg.get_exciter_type(),
        Nodes::Network::WaveguideNetwork::ExciterType::NOISE_BURST);

    wg.set_exciter_type(Nodes::Network::WaveguideNetwork::ExciterType::IMPULSE);
    EXPECT_EQ(wg.get_exciter_type(),
        Nodes::Network::WaveguideNetwork::ExciterType::IMPULSE);

    wg.set_exciter_type(Nodes::Network::WaveguideNetwork::ExciterType::FILTERED_NOISE);
    wg.set_exciter_filter(std::make_shared<Nodes::Filters::FIR>(
        std::vector<double> { 0.25, 0.5, 0.25 }));
    wg.set_exciter_filter(std::make_shared<Nodes::Filters::IIR>(
        std::vector<double> { 1.0, -0.9 }, std::vector<double> { 0.1 }));

    wg.set_exciter_type(Nodes::Network::WaveguideNetwork::ExciterType::SAMPLE);
    wg.set_exciter_sample(std::vector<double>(32, 0.5));

    wg.set_exciter_type(Nodes::Network::WaveguideNetwork::ExciterType::CONTINUOUS);
    wg.set_exciter_node(std::make_shared<Nodes::Generator::Sine>(5.0, 0.2));
}

//-----------------------------------------------------------------------------
// Processing Tests - No engine required
//-----------------------------------------------------------------------------

TEST_F(WaveguideNetworkUnitTest, ExcitationBehavior)
{
    Nodes::Network::WaveguideNetwork wg_silent(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    wg_silent.process_batch(TestConfig::BUFFER_SIZE);
    auto silent_buf = wg_silent.get_audio_buffer();
    ASSERT_TRUE(silent_buf.has_value());
    double max_silent = 0.0;
    for (double s : *silent_buf)
        max_silent = std::max(max_silent, std::abs(s));
    EXPECT_LT(max_silent, 1e-10);

    Nodes::Network::WaveguideNetwork wg_pluck(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    wg_pluck.pluck(0.5, 1.0);
    wg_pluck.process_batch(TestConfig::BUFFER_SIZE);
    auto pluck_buf = wg_pluck.get_audio_buffer();
    ASSERT_TRUE(pluck_buf.has_value());
    ASSERT_EQ(pluck_buf->size(), TestConfig::BUFFER_SIZE);
    double max_pluck = 0.0;
    for (double s : *pluck_buf)
        max_pluck = std::max(max_pluck, std::abs(s));
    EXPECT_GT(max_pluck, 0.001);

    Nodes::Network::WaveguideNetwork wg_strike(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    wg_strike.strike(0.5, 1.0);
    wg_strike.process_batch(TestConfig::BUFFER_SIZE);
    auto strike_buf = wg_strike.get_audio_buffer();
    ASSERT_TRUE(strike_buf.has_value());
    double max_strike = 0.0;
    for (double s : *strike_buf)
        max_strike = std::max(max_strike, std::abs(s));
    EXPECT_GT(max_strike, 0.001);
}

TEST_F(WaveguideNetworkUnitTest, TubeExcitationAndBidirectionalOutput)
{
    Nodes::Network::WaveguideNetwork tube(
        Nodes::Network::WaveguideNetwork::WaveguideType::TUBE, 220.0, TestConfig::SAMPLE_RATE);

    tube.strike(0.1, 1.0);
    tube.process_batch(TestConfig::BUFFER_SIZE);

    auto buf = tube.get_audio_buffer();
    ASSERT_TRUE(buf.has_value());
    ASSERT_EQ(buf->size(), TestConfig::BUFFER_SIZE);

    double max_abs = 0.0;
    for (double s : *buf)
        max_abs = std::max(max_abs, std::abs(s));
    EXPECT_GT(max_abs, 0.001);

    const auto& seg = tube.get_segments()[0];
    double p_minus_energy = 0.0;
    for (size_t i = 0; i < seg.p_minus.capacity(); ++i) {
        p_minus_energy += seg.p_minus[i] * seg.p_minus[i];
    }
    EXPECT_GT(p_minus_energy, 0.0);
}

TEST_F(WaveguideNetworkUnitTest, TubeAndStringProduceDifferentOutput)
{
    Nodes::Network::WaveguideNetwork str(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    Nodes::Network::WaveguideNetwork tube(
        Nodes::Network::WaveguideNetwork::WaveguideType::TUBE, 220.0, TestConfig::SAMPLE_RATE);

    str.pluck(0.5, 1.0);
    tube.pluck(0.5, 1.0);

    for (int i = 0; i < 10; ++i) {
        str.process_batch(TestConfig::BUFFER_SIZE);
        tube.process_batch(TestConfig::BUFFER_SIZE);
    }

    auto buf_str = str.get_audio_buffer();
    auto buf_tube = tube.get_audio_buffer();
    ASSERT_TRUE(buf_str.has_value());
    ASSERT_TRUE(buf_tube.has_value());

    double e_str = 0.0, e_tube = 0.0;
    for (size_t i = 0; i < TestConfig::BUFFER_SIZE; ++i) {
        e_str += (*buf_str)[i] * (*buf_str)[i];
        e_tube += (*buf_tube)[i] * (*buf_tube)[i];
    }
    EXPECT_NE(e_str, e_tube);
}

TEST_F(WaveguideNetworkUnitTest, TubePerEndFiltersAffectTimbre)
{
    Nodes::Network::WaveguideNetwork tube_default(
        Nodes::Network::WaveguideNetwork::WaveguideType::TUBE, 220.0, TestConfig::SAMPLE_RATE);
    Nodes::Network::WaveguideNetwork tube_custom(
        Nodes::Network::WaveguideNetwork::WaveguideType::TUBE, 220.0, TestConfig::SAMPLE_RATE);

    tube_custom.set_loop_filter_open(std::make_shared<Nodes::Filters::FIR>(
        std::vector<double> { 0.2, 0.6, 0.2 }));

    tube_default.strike(0.1, 1.0);
    tube_custom.strike(0.1, 1.0);

    for (int i = 0; i < 20; ++i) {
        tube_default.process_batch(TestConfig::BUFFER_SIZE);
        tube_custom.process_batch(TestConfig::BUFFER_SIZE);
    }

    auto buf_def = tube_default.get_audio_buffer();
    auto buf_cust = tube_custom.get_audio_buffer();
    ASSERT_TRUE(buf_def.has_value());
    ASSERT_TRUE(buf_cust.has_value());

    double e_def = 0.0, e_cust = 0.0;
    for (size_t i = 0; i < TestConfig::BUFFER_SIZE; ++i) {
        e_def += (*buf_def)[i] * (*buf_def)[i];
        e_cust += (*buf_cust)[i] * (*buf_cust)[i];
    }
    EXPECT_NE(e_def, e_cust);
}

TEST_F(WaveguideNetworkUnitTest, PluckPositionAndDecayBehavior)
{
    Nodes::Network::WaveguideNetwork wg_center(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    Nodes::Network::WaveguideNetwork wg_bridge(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);

    wg_center.pluck(0.5, 1.0);
    wg_bridge.pluck(0.1, 1.0);
    wg_center.process_batch(TestConfig::BUFFER_SIZE);
    wg_bridge.process_batch(TestConfig::BUFFER_SIZE);

    double e_center = 0.0, e_bridge = 0.0;
    auto bc = wg_center.get_audio_buffer();
    auto bb = wg_bridge.get_audio_buffer();
    ASSERT_TRUE(bc.has_value());
    ASSERT_TRUE(bb.has_value());
    for (size_t i = 0; i < TestConfig::BUFFER_SIZE; ++i) {
        e_center += (*bc)[i] * (*bc)[i];
        e_bridge += (*bb)[i] * (*bb)[i];
    }
    EXPECT_NE(e_center, e_bridge);

    Nodes::Network::WaveguideNetwork wg(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    wg.pluck(0.5, 1.0);
    wg.process_batch(TestConfig::BUFFER_SIZE);
    auto early = wg.get_audio_buffer();
    for (int i = 0; i < 100; ++i)
        wg.process_batch(TestConfig::BUFFER_SIZE);
    auto late = wg.get_audio_buffer();

    double e_early = 0.0, e_late = 0.0;
    for (size_t i = 0; i < TestConfig::BUFFER_SIZE; ++i) {
        e_early += (*early)[i] * (*early)[i];
        e_late += (*late)[i] * (*late)[i];
    }
    EXPECT_GT(e_early, e_late);
}

TEST_F(WaveguideNetworkUnitTest, LossFactorAndLoopFilterAffectTimbre)
{
    Nodes::Network::WaveguideNetwork wg_short(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    Nodes::Network::WaveguideNetwork wg_long(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    wg_short.set_loss_factor(0.98);
    wg_long.set_loss_factor(0.999);
    wg_short.pluck(0.5, 1.0);
    wg_long.pluck(0.5, 1.0);
    for (int i = 0; i < 50; ++i) {
        wg_short.process_batch(TestConfig::BUFFER_SIZE);
        wg_long.process_batch(TestConfig::BUFFER_SIZE);
    }
    double e_short = 0.0, e_long = 0.0;
    for (size_t i = 0; i < TestConfig::BUFFER_SIZE; ++i) {
        e_short += (*wg_short.get_audio_buffer())[i] * (*wg_short.get_audio_buffer())[i];
        e_long += (*wg_long.get_audio_buffer())[i] * (*wg_long.get_audio_buffer())[i];
    }
    EXPECT_GT(e_long, e_short);

    Nodes::Network::WaveguideNetwork wg_default(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    Nodes::Network::WaveguideNetwork wg_filtered(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);
    wg_filtered.set_loop_filter(std::make_shared<Nodes::Filters::FIR>(
        std::vector<double> { 0.25, 0.5, 0.25 }));
    wg_default.pluck(0.5, 1.0);
    wg_filtered.pluck(0.5, 1.0);
    for (int i = 0; i < 20; ++i) {
        wg_default.process_batch(TestConfig::BUFFER_SIZE);
        wg_filtered.process_batch(TestConfig::BUFFER_SIZE);
    }
    double e_def = 0.0, e_filt = 0.0;
    for (size_t i = 0; i < TestConfig::BUFFER_SIZE; ++i) {
        e_def += (*wg_default.get_audio_buffer())[i] * (*wg_default.get_audio_buffer())[i];
        e_filt += (*wg_filtered.get_audio_buffer())[i] * (*wg_filtered.get_audio_buffer())[i];
    }
    EXPECT_NE(e_def, e_filt);
}

TEST_F(WaveguideNetworkUnitTest, DisabledAndResetBehavior)
{
    Nodes::Network::WaveguideNetwork wg(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, TestConfig::SAMPLE_RATE);

    wg.pluck(0.5, 1.0);
    wg.set_enabled(false);
    wg.process_batch(TestConfig::BUFFER_SIZE);
    auto disabled_buf = wg.get_audio_buffer();
    ASSERT_TRUE(disabled_buf.has_value());
    double max_disabled = 0.0;
    for (double s : *disabled_buf)
        max_disabled = std::max(max_disabled, std::abs(s));
    EXPECT_LT(max_disabled, 1e-10);

    wg.set_enabled(true);
    wg.pluck(0.5, 1.0);
    wg.process_batch(TestConfig::BUFFER_SIZE);
    double e_before = 0.0;
    for (double s : *wg.get_audio_buffer())
        e_before += s * s;
    EXPECT_GT(e_before, 0.0);

    wg.reset();
    wg.process_batch(TestConfig::BUFFER_SIZE);
    double e_after = 0.0;
    for (double s : *wg.get_audio_buffer())
        e_after += s * s;
    EXPECT_LT(e_after, e_before * 0.01);
}

//-----------------------------------------------------------------------------
// Integration Tests - Engine required
//-----------------------------------------------------------------------------

class WaveguideNetworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        MayaFlux::Init();
        AudioTestHelper::waitForAudio(100);
        MayaFlux::Start();
        AudioTestHelper::waitForAudio(100);
        node_manager = MayaFlux::get_node_graph_manager();
    }

    void TearDown() override
    {
        MayaFlux::End();
    }

    std::shared_ptr<Nodes::NodeGraphManager> node_manager;
};

TEST_F(WaveguideNetworkIntegrationTest, PluckAndStrikeWithEngineProcessing)
{
    auto wg = std::make_shared<Nodes::Network::WaveguideNetwork>(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, 48000.0);

    node_manager->add_network(wg, Nodes::ProcessingToken::AUDIO_RATE);

    wg->pluck(0.5, 1.0);
    wg->process_batch(512);
    auto pluck_buf = wg->get_audio_buffer();
    ASSERT_TRUE(pluck_buf.has_value());
    EXPECT_EQ(pluck_buf->size(), 512);
    EXPECT_TRUE(std::any_of(pluck_buf->begin(), pluck_buf->end(),
        [](double s) { return std::abs(s) > 0.001; }));

    wg->reset();
    wg->strike(0.5, 1.0);
    wg->process_batch(512);
    auto strike_buf = wg->get_audio_buffer();
    ASSERT_TRUE(strike_buf.has_value());
    EXPECT_TRUE(std::any_of(strike_buf->begin(), strike_buf->end(),
        [](double s) { return std::abs(s) > 0.001; }));
}

TEST_F(WaveguideNetworkIntegrationTest, ContinuousExciterProducesSustainedOutput)
{
    auto wg = std::make_shared<Nodes::Network::WaveguideNetwork>(
        Nodes::Network::WaveguideNetwork::WaveguideType::STRING, 220.0, 48000.0);

    node_manager->add_network(wg, Nodes::ProcessingToken::AUDIO_RATE);

    wg->set_exciter_node(std::make_shared<Nodes::Generator::Sine>(55.0F));
    wg->set_exciter_type(Nodes::Network::WaveguideNetwork::ExciterType::CONTINUOUS);
    wg->strike(0.5, 0.3);

    AudioTestHelper::waitForAudio(2000);

    auto buffer = wg->get_audio_buffer();
    ASSERT_TRUE(buffer.has_value());
    double energy = 0.0;
    for (double s : *buffer)
        energy += s * s;
    EXPECT_GT(energy, 0.0001);
}

} // namespace MayaFlux::Test
