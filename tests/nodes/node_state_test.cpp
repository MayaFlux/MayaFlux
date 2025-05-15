#include "../test_config.h"

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

TEST_F(NodeProcessStateTest, BasicRegistrationState)
{
    EXPECT_FALSE(sine->is_registered_for_processing());
    EXPECT_FALSE(fir->is_registered_for_processing());

    sine->mark_registered_for_processing(true);
    fir->mark_registered_for_processing(true);

    EXPECT_TRUE(sine->is_registered_for_processing());
    EXPECT_TRUE(fir->is_registered_for_processing());

    sine->mark_registered_for_processing(false);
    fir->mark_registered_for_processing(false);

    EXPECT_FALSE(sine->is_registered_for_processing());
    EXPECT_FALSE(fir->is_registered_for_processing());
}

TEST_F(NodeProcessStateTest, ProcessedState)
{
    EXPECT_FALSE(sine->is_processed());
    EXPECT_FALSE(fir->is_processed());

    sine->mark_processed(true);
    fir->mark_processed(true);

    EXPECT_TRUE(sine->is_processed());
    EXPECT_TRUE(fir->is_processed());

    sine->mark_processed(false);
    fir->mark_processed(false);

    EXPECT_FALSE(sine->is_processed());
    EXPECT_FALSE(fir->is_processed());
}

TEST_F(NodeProcessStateTest, ChainNodeInitialization)
{
    auto chain = std::make_shared<Nodes::ChainNode>(sine, fir);

    EXPECT_FALSE(chain->is_initialized());

    sine->mark_registered_for_processing(true);
    fir->mark_registered_for_processing(true);

    chain->mark_registered_for_processing(true);

    EXPECT_FALSE(chain->is_initialized());

    sine->mark_registered_for_processing(false);
    EXPECT_FALSE(chain->is_initialized());

    chain->initialize();

    EXPECT_FALSE(sine->is_registered_for_processing());
    EXPECT_FALSE(fir->is_registered_for_processing());
    EXPECT_TRUE(chain->is_initialized());
}

TEST_F(NodeProcessStateTest, BinaryOpNodeInitialization)
{
    auto binary_op = std::make_shared<Nodes::BinaryOpNode>(
        sine, fir, [](double a, double b) { return a + b; });

    EXPECT_FALSE(binary_op->is_registered_for_processing());

    sine->mark_registered_for_processing(true);
    fir->mark_registered_for_processing(true);
    binary_op->mark_registered_for_processing(true);

    binary_op->initialize();

    EXPECT_FALSE(sine->is_registered_for_processing());
    EXPECT_FALSE(fir->is_registered_for_processing());
    EXPECT_TRUE(binary_op->is_registered_for_processing());
}

TEST_F(NodeProcessStateTest, ChainNodeProcessing)
{
    auto chain = std::make_shared<Nodes::ChainNode>(sine, fir);
    chain->initialize();

    double input = 0.5;
    double output = chain->process_sample(input);

    EXPECT_DOUBLE_EQ(output, chain->get_last_output());

    // Only engine updates the states
    EXPECT_FALSE(chain->is_processed());
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
    EXPECT_FALSE(binary_op->is_processed());
}

TEST_F(NodeProcessStateTest, ComplexChaining)
{
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(220.0f, 0.5f);
    auto sine2 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.3f);
    auto sine3 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.2f);

    auto sum = sine2 + sine3;
    EXPECT_TRUE(sum->is_registered_for_processing());

    auto chain = sine1 >> sum;

    EXPECT_FALSE(sine1->is_registered_for_processing());
    EXPECT_FALSE(sine2->is_registered_for_processing());
    EXPECT_FALSE(sine3->is_registered_for_processing());
    EXPECT_FALSE(sum->is_registered_for_processing());
    EXPECT_TRUE(chain->is_registered_for_processing());

    double input = 0.5;
    double output = chain->process_sample(input);

    EXPECT_TRUE(sine1->is_processed());
    EXPECT_TRUE(sine2->is_processed());
    EXPECT_TRUE(sine3->is_processed());
    EXPECT_TRUE(sum->is_processed());
    EXPECT_FALSE(chain->is_processed());

    double sine1_output = sine1->get_last_output();
    double sine2_output = sine2->get_last_output();
    double sine3_output = sine3->get_last_output();
    // double expected_sum =

    // EXPECT_DOUBLE_EQ(sum->get_last_output(), expected_sum);
    EXPECT_DOUBLE_EQ(output, chain->get_last_output());
}

TEST_F(NodeProcessStateTest, CyclicProcessingPrevention)
{
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(220.0f, 0.5f);
    auto sine2 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.3f);

    auto chain1 = sine1 >> sine2;
    auto chain2 = sine2 >> sine1;

    std::dynamic_pointer_cast<Nodes::ChainNode>(chain1)->initialize();
    std::dynamic_pointer_cast<Nodes::ChainNode>(chain2)->initialize();

    chain1->process_sample(0.5);

    EXPECT_TRUE(sine1->is_processed());
    EXPECT_TRUE(sine2->is_processed());
    EXPECT_FALSE(chain1->is_processed());

    sine1->mark_processed(false);
    sine2->mark_processed(false);
    chain1->mark_processed(false);
    chain2->mark_processed(false);

    chain2->process_sample(0.5);

    EXPECT_TRUE(sine1->is_processed());
    EXPECT_TRUE(sine2->is_processed());
    EXPECT_FALSE(chain2->is_processed());
}

TEST_F(NodeProcessStateTest, ProcessBatchWithState)
{
    auto chain = std::make_shared<Nodes::ChainNode>(sine, fir);
    chain->initialize();

    unsigned int num_samples = 100;
    std::vector<double> output = chain->process_batch(num_samples);

    EXPECT_EQ(output.size(), num_samples);

    EXPECT_FALSE(chain->is_processed());
    EXPECT_TRUE(sine->is_processed());
    EXPECT_TRUE(fir->is_processed());

    chain->mark_processed(false);
    sine->mark_processed(false);
    fir->mark_processed(false);

    EXPECT_FALSE(chain->is_processed());
    EXPECT_FALSE(sine->is_processed());
    EXPECT_FALSE(fir->is_processed());
}

}
