#include "../test_config.h"

#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Nodes/NodeStructure.hpp"

namespace MayaFlux::Test {

class NodeProcessStateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
        fir_coeffs = { 0.2, 0.2, 0.2, 0.2, 0.2 };
        fir = std::make_shared<Nodes::Filters::FIR>(sine, fir_coeffs);
    }

    std::shared_ptr<Nodes::Generator::Sine> sine;
    std::vector<double> fir_coeffs;
    std::shared_ptr<Nodes::Filters::FIR> fir;
};

TEST_F(NodeProcessStateTest, ModulatorCounterBasics)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto consumer = std::make_shared<Nodes::Filters::FIR>(modulator, fir_coeffs);

    EXPECT_EQ(modulator->m_modulator_count.load(), 0);

    Nodes::atomic_inc_modulator_count(modulator->m_modulator_count, 1);
    EXPECT_EQ(modulator->m_modulator_count.load(), 1);

    Nodes::atomic_dec_modulator_count(modulator->m_modulator_count, 1);
    EXPECT_EQ(modulator->m_modulator_count.load(), 0);
}

TEST_F(NodeProcessStateTest, ModulatorAutoReset)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto consumer = std::make_shared<Nodes::Filters::FIR>(modulator, fir_coeffs);

    Nodes::atomic_add_flag(modulator->m_state, Utils::NodeState::PROCESSED);
    EXPECT_TRUE(modulator->m_state.load() & Utils::NodeState::PROCESSED);

    consumer->process_sample(0.0);

    EXPECT_FALSE(modulator->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, MultipleConsumerCoordination)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto consumer1 = std::make_shared<Nodes::Filters::FIR>(modulator, fir_coeffs);
    auto consumer2 = std::make_shared<Nodes::Generator::Sine>(modulator, 880.0f, 0.5f);

    Nodes::atomic_add_flag(modulator->m_state, Utils::NodeState::PROCESSED);

    consumer1->process_sample(0.0);
    EXPECT_FALSE(modulator->m_state.load() & Utils::NodeState::PROCESSED);

    consumer2->process_sample(0.0);
    EXPECT_FALSE(modulator->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, RootNodeOwnershipTrumpsCounter)
{
    auto node = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    MayaFlux::register_audio_node(node);
    EXPECT_TRUE(node->m_state.load() & Utils::NodeState::ACTIVE);

    Nodes::atomic_add_flag(node->m_state, Utils::NodeState::PROCESSED);

    node->request_reset_from_channel(0);
    EXPECT_FALSE(node->m_state.load() & Utils::NodeState::PROCESSED);

    MayaFlux::unregister_audio_node(node);
}

TEST_F(NodeProcessStateTest, FunctionalCorrectness)
{
    auto freq_mod = std::make_shared<Nodes::Generator::Sine>(5.0f, 50.0f);
    auto carrier = std::make_shared<Nodes::Generator::Sine>(freq_mod, 440.0f, 0.5f);

    std::vector<double> samples;
    for (int i = 0; i < 100; ++i) {
        samples.push_back(carrier->process_sample(0.0));
    }

    bool has_variation = false;
    for (size_t i = 1; i < samples.size(); ++i) {
        if (std::abs(samples[i] - samples[i - 1]) > 0.01) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation);
}

TEST_F(NodeProcessStateTest, RealisticProcessingCycle)
{
    auto freq_mod = std::make_shared<Nodes::Generator::Sine>(5.0f, 100.0f);
    auto amp_mod = std::make_shared<Nodes::Generator::Sine>(3.0f, 0.3f);
    auto carrier = std::make_shared<Nodes::Generator::Sine>(freq_mod, amp_mod);

    MayaFlux::register_audio_node(carrier);

    const int buffer_size = 512;
    // MayaFlux::get_audio_channel_root().process_sample();
    std::vector<double> output = MayaFlux::get_audio_channel_root().process_batch(buffer_size);

    EXPECT_EQ(output.size(), buffer_size);

    bool has_signal = false;
    for (double sample : output) {
        if (std::abs(sample) > 0.001) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);

    MayaFlux::unregister_audio_node(carrier);
}

TEST_F(NodeProcessStateTest, CounterEdgeCases)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    EXPECT_EQ(modulator->m_modulator_count.load(), 0);

    Nodes::atomic_inc_modulator_count(modulator->m_modulator_count, 1);
    Nodes::atomic_inc_modulator_count(modulator->m_modulator_count, 1);
    EXPECT_EQ(modulator->m_modulator_count.load(), 2);

    modulator->m_modulator_count.store(0);
}

}
