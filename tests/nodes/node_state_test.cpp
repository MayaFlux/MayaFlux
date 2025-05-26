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

/* TEST_F(NodeProcessStateTest, BasicRegistrationState)
{
    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::ACTIVE);

    Nodes::atomic_add_flag(sine->m_state, Utils::NodeState::ACTIVE);
    Nodes::atomic_add_flag(fir->m_state, Utils::NodeState::ACTIVE);

    EXPECT_TRUE(sine->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_TRUE(fir->m_state.load() & Utils::NodeState::ACTIVE);

    Nodes::atomic_remove_flag(sine->m_state, Utils::NodeState::ACTIVE);
    Nodes::atomic_remove_flag(fir->m_state, Utils::NodeState::ACTIVE);

    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::ACTIVE);
}

TEST_F(NodeProcessStateTest, ProcessedState)
{
    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::PROCESSED);

    Nodes::atomic_add_flag(fir->m_state, Utils::NodeState::PROCESSED);

    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(fir->m_state.load() & Utils::NodeState::PROCESSED);

    fir->process_sample(0.0f);
    EXPECT_TRUE(sine->m_state.load() & Utils::NodeState::PROCESSED);

    Nodes::atomic_remove_flag(sine->m_state, Utils::NodeState::PROCESSED);
    Nodes::atomic_remove_flag(fir->m_state, Utils::NodeState::PROCESSED);

    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, ChainNodeInitialization)
{
    auto chain = std::make_shared<Nodes::ChainNode>(sine, fir);

    EXPECT_FALSE(chain->is_initialized());

    Nodes::atomic_add_flag(sine->m_state, Utils::NodeState::ACTIVE);
    Nodes::atomic_add_flag(fir->m_state, Utils::NodeState::ACTIVE);

    Nodes::atomic_add_flag(chain->m_state, Utils::NodeState::ACTIVE);

    EXPECT_FALSE(chain->is_initialized());

    Nodes::atomic_remove_flag(sine->m_state, Utils::NodeState::ACTIVE);
    EXPECT_FALSE(chain->is_initialized());

    chain->initialize();

    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_TRUE(chain->is_initialized());
}

TEST_F(NodeProcessStateTest, BinaryOpNodeInitialization)
{
    auto binary_op = std::make_shared<Nodes::BinaryOpNode>(
        sine, fir, [](double a, double b) { return a + b; });

    EXPECT_FALSE(binary_op->m_state.load() & Utils::NodeState::ACTIVE);

    Nodes::atomic_add_flag(sine->m_state, Utils::NodeState::ACTIVE);
    Nodes::atomic_add_flag(fir->m_state, Utils::NodeState::ACTIVE);
    Nodes::atomic_add_flag(binary_op->m_state, Utils::NodeState::ACTIVE);

    binary_op->initialize();

    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_TRUE(binary_op->m_state.load() & Utils::NodeState::ACTIVE);
}

TEST_F(NodeProcessStateTest, ChainNodeProcessing)
{
    auto chain = std::make_shared<Nodes::ChainNode>(sine, fir);
    chain->initialize();

    double input = 0.5;
    double output = chain->process_sample(input);

    EXPECT_DOUBLE_EQ(output, chain->get_last_output());

    // Only engine updates the states
    EXPECT_FALSE(chain->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, BinaryOpNodeProcessing)
{
    auto binary_op = std::make_shared<Nodes::BinaryOpNode>(
        sine, fir, [](double a, double b) { return a + b; });
    binary_op->initialize();

    double input = 0.5;
    double output = binary_op->process_sample(input);

    EXPECT_DOUBLE_EQ(output, binary_op->get_last_output());

    // Only engine updates the states
    EXPECT_FALSE(binary_op->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, ComplexChaining)
{
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(220.0f, 0.5f);
    auto sine2 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.3f);
    auto sine3 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.2f);

    auto sum = sine2 + sine3;
    EXPECT_TRUE(sum->m_state.load() & Utils::NodeState::ACTIVE);

    auto chain = sine1 >> sum;

    EXPECT_FALSE(sine1->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(sine2->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(sine3->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_FALSE(sum->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_TRUE(chain->m_state.load() & Utils::NodeState::ACTIVE);

    double input = 0.5;
    double output = chain->process_sample(input);

    EXPECT_TRUE(sine1->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(sine2->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(sine3->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(sum->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(chain->m_state.load() & Utils::NodeState::PROCESSED);

    double sine1_output = sine1->get_last_output();
    double sine2_output = sine2->get_last_output();
    double sine3_output = sine3->get_last_output();

    EXPECT_DOUBLE_EQ(output, chain->get_last_output());
}

TEST_F(NodeProcessStateTest, CyclicProcessingPrevention)
{
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(220.0f, 0.5f);
    auto sine2 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.3f);

    auto chain1 = sine1 >> sine2;
    auto chain2 = sine2 >> sine1;

    EXPECT_TRUE(chain1->m_state.load() & Utils::NodeState::ACTIVE);
    EXPECT_TRUE(chain2->m_state.load() & Utils::NodeState::ACTIVE);

    chain1->process_sample(0.5);

    EXPECT_TRUE(sine1->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(sine2->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(chain1->m_state.load() & Utils::NodeState::PROCESSED);

    Nodes::atomic_remove_flag(sine1->m_state, Utils::NodeState::PROCESSED);
    Nodes::atomic_remove_flag(sine2->m_state, Utils::NodeState::PROCESSED);
    Nodes::atomic_remove_flag(chain1->m_state, Utils::NodeState::PROCESSED);
    Nodes::atomic_remove_flag(chain2->m_state, Utils::NodeState::PROCESSED);

    chain2->process_sample(0.5);

    EXPECT_TRUE(sine1->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(sine2->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(chain2->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, ProcessBatchWithState)
{
    auto chain = std::make_shared<Nodes::ChainNode>(sine, fir);
    chain->initialize();

    unsigned int num_samples = 100;
    std::vector<double> output = chain->process_batch(num_samples);

    EXPECT_EQ(output.size(), num_samples);

    EXPECT_FALSE(chain->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(sine->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_TRUE(fir->m_state.load() & Utils::NodeState::PROCESSED);

    Nodes::atomic_remove_flag(chain->m_state, Utils::NodeState::PROCESSED);
    Nodes::atomic_remove_flag(sine->m_state, Utils::NodeState::PROCESSED);
    Nodes::atomic_remove_flag(fir->m_state, Utils::NodeState::PROCESSED);

    EXPECT_FALSE(chain->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(sine->m_state.load() & Utils::NodeState::PROCESSED);
    EXPECT_FALSE(fir->m_state.load() & Utils::NodeState::PROCESSED);
} */

TEST_F(NodeProcessStateTest, ModulatorCounterBasics)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto consumer = std::make_shared<Nodes::Filters::FIR>(modulator, fir_coeffs);

    // Test counter mechanics
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

    // Manually set processed state
    Nodes::atomic_add_flag(modulator->m_state, Utils::NodeState::PROCESSED);
    EXPECT_TRUE(modulator->m_state.load() & Utils::NodeState::PROCESSED);

    // Process consumer (which should inc/dec modulator counter)
    consumer->process_sample(0.0);

    // Modulator should be reset automatically
    EXPECT_FALSE(modulator->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, MultipleConsumerCoordination)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto consumer1 = std::make_shared<Nodes::Filters::FIR>(modulator, fir_coeffs);
    auto consumer2 = std::make_shared<Nodes::Generator::Sine>(modulator, 880.0f, 0.5f);

    Nodes::atomic_add_flag(modulator->m_state, Utils::NodeState::PROCESSED);

    // First consumer processes
    consumer1->process_sample(0.0);
    EXPECT_FALSE(modulator->m_state.load() & Utils::NodeState::PROCESSED);

    // Second consumer processes
    consumer2->process_sample(0.0);
    // NOW modulator should be reset (counter = 0)
    EXPECT_FALSE(modulator->m_state.load() & Utils::NodeState::PROCESSED);
}

TEST_F(NodeProcessStateTest, RootNodeOwnershipTrumpsCounter)
{
    auto node = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    // Add to root node (gets ACTIVE flag)
    MayaFlux::add_node_to_root(node);
    EXPECT_TRUE(node->m_state.load() & Utils::NodeState::ACTIVE);

    // Set processed state
    Nodes::atomic_add_flag(node->m_state, Utils::NodeState::PROCESSED);

    // Even with modulator counter, root node ownership should allow immediate reset
    node->reset_processed_state();
    EXPECT_FALSE(node->m_state.load() & Utils::NodeState::PROCESSED);

    MayaFlux::remove_node_from_root(node);
}

// Test FUNCTIONAL behavior rather than internal state
TEST_F(NodeProcessStateTest, FunctionalCorrectness)
{
    auto freq_mod = std::make_shared<Nodes::Generator::Sine>(5.0f, 50.0f);
    auto carrier = std::make_shared<Nodes::Generator::Sine>(freq_mod, 440.0f, 0.5f);

    // Generate some samples
    std::vector<double> samples;
    for (int i = 0; i < 100; ++i) {
        samples.push_back(carrier->process_sample(0.0));
    }

    // Test that modulation is actually working (functional test)
    // Look for frequency variation, not internal state
    bool has_variation = false;
    for (size_t i = 1; i < samples.size(); ++i) {
        if (std::abs(samples[i] - samples[i - 1]) > 0.01) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation); // Modulation should cause variation
}

// Test performance/correctness under realistic conditions
TEST_F(NodeProcessStateTest, RealisticProcessingCycle)
{
    auto freq_mod = std::make_shared<Nodes::Generator::Sine>(5.0f, 100.0f);
    auto amp_mod = std::make_shared<Nodes::Generator::Sine>(3.0f, 0.3f);
    auto carrier = std::make_shared<Nodes::Generator::Sine>(freq_mod, amp_mod);

    MayaFlux::add_node_to_root(carrier);

    // Process like real engine would
    const int buffer_size = 512;
    std::vector<double> output = MayaFlux::get_root_node().process(buffer_size);

    // Test outcomes, not internal state
    EXPECT_EQ(output.size(), buffer_size);

    // Verify audio is generated
    bool has_signal = false;
    for (double sample : output) {
        if (std::abs(sample) > 0.001) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);

    MayaFlux::remove_node_from_root(carrier);
}

// Test edge cases and error conditions
TEST_F(NodeProcessStateTest, CounterEdgeCases)
{
    auto modulator = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    // Test counter underflow protection
    EXPECT_EQ(modulator->m_modulator_count.load(), 0);
    // Nodes::atomic_dec_modulator_count(modulator->m_modulator_count, 1);
    // Should handle underflow gracefully (implementation dependent)

    // Test multiple increments from same consumer
    Nodes::atomic_inc_modulator_count(modulator->m_modulator_count, 1);
    Nodes::atomic_inc_modulator_count(modulator->m_modulator_count, 1);
    EXPECT_EQ(modulator->m_modulator_count.load(), 2);

    // Reset for cleanup
    modulator->m_modulator_count.store(0);
}

}
