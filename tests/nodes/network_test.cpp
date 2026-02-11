#include <algorithm>

#include "../test_config.h"

#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Filters/IIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Network/ModalNetwork.hpp"

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
        double expected_spatial_factor = std::abs(std::sin((i + 1) * M_PI * position));
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
// Integration Tests - Engine required (max 2-3)
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

} // namespace MayaFlux::Test
