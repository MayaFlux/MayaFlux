#include "../test_config.h"

#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Filters/IIR.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"

#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {
class NodeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        node_manager = std::make_shared<Nodes::NodeGraphManager>();
    }

    std::shared_ptr<Nodes::NodeGraphManager> node_manager;
};

TEST_F(NodeTest, RootNodeOperations)
{
    auto& root = node_manager->get_root_node();
    EXPECT_EQ(root.get_node_size(), 0);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    root.register_node(sine);
    EXPECT_EQ(root.get_node_size(), 1);

    root.unregister_node(sine);
    EXPECT_EQ(root.get_node_size(), 0);
}

TEST_F(NodeTest, NodeRegistry)
{
    const std::string node_id = "test_sine";
    auto sine = node_manager->create_node<Nodes::Generator::Sine>(node_id, 440.0f, 0.5f);

    EXPECT_NE(sine, nullptr);

    auto retrieved = node_manager->get_node(node_id);
    EXPECT_EQ(retrieved, sine);

    auto nonexistent = node_manager->get_node("nonexistent");
    EXPECT_EQ(nonexistent, nullptr);
}

TEST_F(NodeTest, MultiChannelRootNodes)
{
    auto& root0 = node_manager->get_root_node(0);
    auto& root1 = node_manager->get_root_node(1);

    EXPECT_NE(&root0, &root1);

    auto sine0 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.5f);

    root0.register_node(sine0);
    root1.register_node(sine1);

    EXPECT_EQ(root0.get_node_size(), 1);
    EXPECT_EQ(root1.get_node_size(), 1);

    const auto& all_roots = node_manager->get_all_channel_root_nodes();
    EXPECT_EQ(all_roots.size(), 2);
}

TEST_F(NodeTest, AddNodeToRoot)
{
    const std::string node_id = "test_sine";
    auto sine = node_manager->create_node<Nodes::Generator::Sine>(node_id, 440.0f, 0.5f);

    node_manager->add_to_root(node_id);
    EXPECT_EQ(node_manager->get_root_node().get_node_size(), 1);

    const std::string node_id2 = "test_sine2";
    auto sine2 = node_manager->create_node<Nodes::Generator::Sine>(node_id2, 880.0f, 0.5f);

    node_manager->add_to_root(node_id2, 1);
    EXPECT_EQ(node_manager->get_root_node(1).get_node_size(), 1);

    auto sine3 = std::make_shared<Nodes::Generator::Sine>(660.0f, 0.5f);
    node_manager->add_to_root(sine3, 2);
    EXPECT_EQ(node_manager->get_root_node(2).get_node_size(), 1);
}

TEST_F(NodeTest, NodeConnections)
{
    const std::string sine_id = "sine";
    const std::string filter_id = "filter";

    auto sine = node_manager->create_node<Nodes::Generator::Sine>(sine_id, 440.0f, 0.5f);
    auto filter = node_manager->create_node<Nodes::Filters::FIR>(filter_id, sine, "5_0");

    node_manager->connect(sine_id, filter_id);

    auto sine2 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.3f);
    auto filter2 = std::make_shared<Nodes::Filters::FIR>(sine2, std::vector<double> { 0.2, 0.2, 0.2, 0.2, 0.2 });

    auto chain_node = sine2 >> filter2;
    EXPECT_NE(chain_node, nullptr);

    auto sine3 = std::make_shared<Nodes::Generator::Sine>(220.0f, 0.4f);
    auto sine4 = std::make_shared<Nodes::Generator::Sine>(330.0f, 0.4f);

    auto add_node = sine3 + sine4;
    EXPECT_NE(add_node, nullptr);

    auto mul_node = sine3 * sine4;
    EXPECT_NE(mul_node, nullptr);
}

class SineNodeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    }

    std::shared_ptr<Nodes::Generator::Sine> sine;
};

TEST_F(SineNodeTest, BasicProperties)
{
    EXPECT_FLOAT_EQ(sine->get_frequency(), 440.0f);
    EXPECT_FLOAT_EQ(sine->get_amplitude(), 0.5f);

    sine->set_frequency(880.0f);
    EXPECT_FLOAT_EQ(sine->get_frequency(), 880.0f);

    sine->set_amplitude(0.7f);
    EXPECT_FLOAT_EQ(sine->get_amplitude(), 0.7f);

    sine->set_params(220.0f, 0.3f, 0.0f);
    EXPECT_FLOAT_EQ(sine->get_frequency(), 220.0f);
    EXPECT_FLOAT_EQ(sine->get_amplitude(), 0.3f);
}

TEST_F(SineNodeTest, AudioGeneration)
{
    double sample = sine->process_sample(0.0);
    EXPECT_GE(sample, -sine->get_amplitude());
    EXPECT_LE(sample, sine->get_amplitude());

    unsigned int buffer_size = 1024;
    std::vector<double> buffer = sine->process_batch(buffer_size);

    EXPECT_EQ(buffer.size(), buffer_size);

    double previous = buffer[0];
    int zero_crossings = 0;

    for (size_t i = 1; i < buffer.size(); i++) {
        if ((previous < 0 && buffer[i] >= 0) || (previous >= 0 && buffer[i] < 0)) {
            zero_crossings++;
        }
        previous = buffer[i];
    }

    int expected_crossings = buffer_size / (TestConfig::SAMPLE_RATE / 440.0 / 2);
    EXPECT_NEAR(zero_crossings, expected_crossings, 2);
}

TEST_F(SineNodeTest, Identity)
{
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    unsigned int buffer_size = 1024;
    std::vector<double> buffer = sine1->process_batch(buffer_size);

    auto sine2 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    std::vector<double> unmod_buffer = sine2->process_batch(buffer_size);

    bool differences_found = false;
    for (size_t i = 0; i < buffer_size; i++) {
        if (std::abs(buffer[i] - unmod_buffer[i]) > 0.01) {
            differences_found = true;
            break;
        }
    }

    EXPECT_FALSE(differences_found);
}

TEST_F(SineNodeTest, Modulation)
{
    auto freq_mod = std::make_shared<Nodes::Generator::Sine>(5.0f, 50.0f);
    sine->set_frequency_modulator(freq_mod);

    unsigned int buffer_size = 1024;
    std::vector<double> buffer = sine->process_batch(buffer_size);

    auto unmod_sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    std::vector<double> unmod_buffer = unmod_sine->process_batch(buffer_size);

    bool differences_found = false;
    for (size_t i = 0; i < buffer_size; i++) {
        if (std::abs(buffer[i] - unmod_buffer[i]) > 0.01) {
            differences_found = true;
            break;
        }
    }

    EXPECT_TRUE(differences_found);

    sine->clear_modulators();
    unmod_sine->reset();

    std::vector<double> no_mod_buffer = sine->process_batch(100);
    std::vector<double> check_buffer = unmod_sine->process_batch(100);

    differences_found = false;
    for (size_t i = 0; i < 100; i++) {
        if (std::abs(no_mod_buffer[i] - check_buffer[i]) > 0.01) {
            differences_found = true;
            break;
        }
    }

    EXPECT_FALSE(differences_found);
}

class FilterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

        fir_coeffs = { 0.2, 0.2, 0.2, 0.2, 0.2 };
        fir = std::make_shared<Nodes::Filters::FIR>(sine, fir_coeffs);

        iir_a_coeffs = { 1.0, -0.9 };
        iir_b_coeffs = { 0.1 };
        iir = std::make_shared<Nodes::Filters::IIR>(sine, iir_a_coeffs, iir_b_coeffs);
    }

    std::shared_ptr<Nodes::Generator::Sine> sine;
    std::vector<double> fir_coeffs;
    std::shared_ptr<Nodes::Filters::FIR> fir;
    std::vector<double> iir_a_coeffs;
    std::vector<double> iir_b_coeffs;
    std::shared_ptr<Nodes::Filters::IIR> iir;
};

TEST_F(FilterTest, FIRBasics)
{
    EXPECT_EQ(fir->get_order(), fir_coeffs.size() - 1);
    EXPECT_EQ(fir->get_current_latency(), fir_coeffs.size() - 1);

    fir->set_bypass(true);
    double input_sample = 0.5;
    double bypass_output = fir->process_sample(input_sample);
    EXPECT_DOUBLE_EQ(bypass_output, input_sample);

    fir->set_bypass(false);

    fir->set_gain(2.0);
    EXPECT_DOUBLE_EQ(fir->get_gain(), 2.0);
}

TEST_F(FilterTest, FIRFiltering)
{
    unsigned int num_samples = 20;
    std::vector<double> impulse(num_samples, 0.0);
    impulse[0] = 1.0;

    auto impulse_filter = std::make_shared<Nodes::Filters::FIR>(nullptr, fir_coeffs);

    std::vector<double> response;
    for (size_t i = 0; i < num_samples; i++) {
        response.push_back(impulse_filter->process_sample(impulse[i]));
    }

    for (size_t i = 0; i < fir_coeffs.size(); i++) {
        if (i < response.size()) {
            EXPECT_NEAR(response[i], fir_coeffs[i], 1e-6);
        }
    }

    for (size_t i = fir_coeffs.size(); i < response.size(); i++) {
        EXPECT_NEAR(response[i], 0.0, 1e-6);
    }

    impulse_filter->reset();
    auto history = impulse_filter->get_input_history();
    for (auto val : history) {
        EXPECT_DOUBLE_EQ(val, 0.0);
    }
}

TEST_F(FilterTest, IIRBasics)
{
    EXPECT_EQ(iir->get_order(), std::max(iir_a_coeffs.size(), iir_b_coeffs.size()) - 1);

    iir->set_bypass(true);
    double input_sample = 0.5;
    double bypass_output = iir->process_sample(input_sample);
    EXPECT_DOUBLE_EQ(bypass_output, input_sample);

    iir->set_bypass(false);
    iir->set_gain(2.0);
    EXPECT_DOUBLE_EQ(iir->get_gain(), 2.0);
}

TEST_F(FilterTest, IIRFiltering)
{
    unsigned int num_samples = 50;
    std::vector<double> step(num_samples, 1.0);

    auto step_filter = std::make_shared<Nodes::Filters::IIR>(nullptr, iir_a_coeffs, iir_b_coeffs);

    std::vector<double> response;
    for (size_t i = 0; i < num_samples; i++) {
        response.push_back(step_filter->process_sample(step[i]));
    }

    EXPECT_NEAR(response[0], iir_b_coeffs[0], 1e-6);

    for (size_t i = 1; i < response.size(); i++) {
        EXPECT_GE(response[i], response[i - 1]);
    }

    EXPECT_NEAR(response.back(), 1.0, 0.01);

    double nyquist = TestConfig::SAMPLE_RATE / 2.0;

    double dc_response = step_filter->get_magnitude_response(0.0, TestConfig::SAMPLE_RATE);
    EXPECT_NEAR(dc_response, 1.0, 0.01);

    double nyquist_response = step_filter->get_magnitude_response(nyquist, TestConfig::SAMPLE_RATE);
    EXPECT_LT(nyquist_response, 0.5);
}

class NoiseGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        noise = std::make_shared<Nodes::Generator::Stochastics::NoiseEngine>();
    }

    std::shared_ptr<Nodes::Generator::Stochastics::NoiseEngine> noise;
};

TEST_F(NoiseGeneratorTest, BasicNoise)
{
    unsigned int num_samples = 1000;
    std::vector<double> samples = noise->process_batch(num_samples);

    EXPECT_EQ(samples.size(), num_samples);

    for (const auto& sample : samples) {
        EXPECT_GE(sample, -1.0);
        EXPECT_LE(sample, 1.0);
    }

    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double mean = sum / num_samples;

    EXPECT_NEAR(mean, 0.0, 0.1);

    noise->set_amplitude(0.5);
    samples = noise->process_batch(num_samples);

    for (const auto& sample : samples) {
        EXPECT_GE(sample, -0.5);
        EXPECT_LE(sample, 0.5);
    }
}

TEST_F(NoiseGeneratorTest, DifferentDistributions)
{
    unsigned int num_samples = 1000;

    noise->set_type(Utils::distribution::NORMAL);
    std::vector<double> normal_samples = noise->process_batch(num_samples);

    noise->set_type(Utils::distribution::EXPONENTIAL);
    noise->random_array(0.0, 1.0, 1);
    std::vector<double> exp_samples = noise->process_batch(num_samples);

    bool distributions_different = false;
    for (size_t i = 0; i < num_samples; i++) {
        if (std::abs(normal_samples[i] - exp_samples[i]) > 0.1) {
            distributions_different = true;
            break;
        }
    }

    EXPECT_TRUE(distributions_different);

    for (const auto& sample : exp_samples) {
        EXPECT_GE(sample, 0.0);
    }
}

TEST_F(NoiseGeneratorTest, RangeControl)
{
    double min = 5.0;
    double max = 10.0;
    unsigned int num_samples = 1000;

    std::vector<double> range_samples = noise->random_array(min, max, num_samples);

    for (const auto& sample : range_samples) {
        EXPECT_GE(sample, min);
        EXPECT_LE(sample, max);
    }
}

class NodeCallbackTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
        noise = std::make_shared<Nodes::Generator::Stochastics::NoiseEngine>();
        fir_coeffs = { 0.2, 0.2, 0.2, 0.2, 0.2 };
        fir = std::make_shared<Nodes::Filters::FIR>(sine, fir_coeffs);
    }

    std::shared_ptr<Nodes::Generator::Sine> sine;
    std::shared_ptr<Nodes::Generator::Stochastics::NoiseEngine> noise;
    std::vector<double> fir_coeffs;
    std::shared_ptr<Nodes::Filters::FIR> fir;
};

TEST_F(NodeCallbackTest, BasicTickCallback)
{
    bool callback_called = false;
    double callback_value = 0.0;

    sine->on_tick([&callback_called, &callback_value](const Nodes::NodeContext& context) {
        callback_called = true;
        callback_value = context.value;
    });

    double sample = sine->process_sample(0.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(callback_value, sample);
}

TEST_F(NodeCallbackTest, ConditionalTickCallback)
{
    bool callback_called = false;
    double callback_value = 0.0;

    sine->on_tick_if(
        [&callback_called, &callback_value](const Nodes::NodeContext& context) {
            callback_called = true;
            callback_value = context.value;
        },
        [](const Nodes::NodeContext& context) {
            return context.value > 0.0;
        });

    bool positive_found = false;
    bool negative_found = false;
    callback_called = false;

    for (int i = 0; i < 100 && (!positive_found || !negative_found); i++) {
        double sample = sine->process_sample(0.0);

        if (sample > 0.0)
            positive_found = true;
        if (sample < 0.0)
            negative_found = true;

        if (sample > 0.0 && !callback_called) {
            EXPECT_TRUE(callback_called);
            EXPECT_DOUBLE_EQ(callback_value, sample);
        }

        if (sample < 0.0) {
            EXPECT_EQ(callback_value, callback_value);
        }
    }

    EXPECT_TRUE(positive_found);
    EXPECT_TRUE(negative_found);
}

TEST_F(NodeCallbackTest, MultipleCallbacks)
{
    int callback1_count = 0;
    int callback2_count = 0;

    sine->on_tick([&callback1_count](const Nodes::NodeContext&) {
        callback1_count++;
    });

    sine->on_tick([&callback2_count](const Nodes::NodeContext&) {
        callback2_count++;
    });

    const int num_samples = 10;
    for (int i = 0; i < num_samples; i++) {
        sine->process_sample(0.0);
    }

    EXPECT_EQ(callback1_count, num_samples);
    EXPECT_EQ(callback2_count, num_samples);
}

TEST_F(NodeCallbackTest, NoiseEngineCallbacks)
{
    bool callback_called = false;
    double callback_value = 0.0;

    noise->on_tick([&callback_called, &callback_value](const Nodes::NodeContext& context) {
        callback_called = true;
        callback_value = context.value;
    });

    double sample = noise->process_sample(0.0);

    EXPECT_TRUE(callback_called);
    EXPECT_DOUBLE_EQ(callback_value, sample);
}

TEST_F(NodeCallbackTest, ConditionalNoiseCallbacks)
{
    int conditional_count = 0;
    int total_count = 0;

    noise->on_tick([&total_count](const Nodes::NodeContext&) {
        total_count++;
    });

    noise->on_tick_if(
        [&conditional_count](const Nodes::NodeContext&) {
            conditional_count++;
        },
        [](const Nodes::NodeContext& context) {
            return context.value > 0.5;
        });

    const int num_samples = 1000;
    for (int i = 0; i < num_samples; i++) {
        noise->process_sample(0.0);
    }

    EXPECT_EQ(total_count, num_samples);
    EXPECT_GT(conditional_count, 0);
    EXPECT_LT(conditional_count, num_samples);
}

TEST_F(NodeCallbackTest, FilterNodeCallbacks)
{
    bool input_callback_called = false;
    bool output_callback_called = false;
    double input_value = 0.0;
    double output_value = 0.0;

    sine->on_tick([&input_callback_called, &input_value](const Nodes::NodeContext& context) {
        input_callback_called = true;
        input_value = context.value;
    });

    fir->on_tick([&output_callback_called, &output_value](const Nodes::NodeContext& context) {
        output_callback_called = true;
        output_value = context.value;
    });

    double sample = fir->process_sample(0.0);

    EXPECT_TRUE(input_callback_called);
    EXPECT_TRUE(output_callback_called);
    EXPECT_DOUBLE_EQ(output_value, sample);
}

TEST_F(NodeCallbackTest, NodeChainCallbacks)
{
    int sine_count = 0;
    int filter_count = 0;

    auto test_sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto test_fir = std::make_shared<Nodes::Filters::FIR>(test_sine, fir_coeffs);

    test_sine->on_tick([&sine_count](const Nodes::NodeContext&) {
        sine_count++;
    });

    test_fir->on_tick([&filter_count](const Nodes::NodeContext&) {
        filter_count++;
    });

    auto chain_node = test_sine >> test_fir;

    MayaFlux::get_node_graph_manager()->add_to_root(chain_node);

    const int num_samples = 10;
    for (int i = 0; i < num_samples; i++) {
        MayaFlux::get_root_node().process();
    }

    EXPECT_EQ(sine_count, num_samples);
    EXPECT_EQ(filter_count, num_samples);

    MayaFlux::get_node_graph_manager()->get_root_node().unregister_node(chain_node);

    chain_node->remove_all_hooks();
    test_sine->remove_all_hooks();
    test_fir->remove_all_hooks();
}

TEST_F(NodeCallbackTest, RemoveHooks)
{
    int sine_count = 0;

    auto callback = [&sine_count](const Nodes::NodeContext&) {
        sine_count++;
    };

    sine->on_tick(callback);

    sine->process_sample(0.0);
    EXPECT_EQ(sine_count, 1);

    bool removed = sine->remove_hook(callback);
    EXPECT_TRUE(removed);

    for (int i = 0; i < 5; i++) {
        sine->process_sample(0.0);
    }
    EXPECT_EQ(sine_count, 1);

    auto nonexistent_callback = [](const Nodes::NodeContext&) { };
    removed = sine->remove_hook(nonexistent_callback);
    EXPECT_FALSE(removed);
}

TEST_F(NodeCallbackTest, RemoveConditionalHooks)
{
    int conditional_count = 0;

    auto condition = [](const Nodes::NodeContext& context) {
        return context.value > 0.0;
    };

    auto callback = [&conditional_count](const Nodes::NodeContext&) {
        conditional_count++;
    };

    sine->on_tick_if(callback, condition);

    for (int i = 0; i < 20; i++) {
        sine->process_sample(0.0);
    }

    int initial_count = conditional_count;
    EXPECT_GT(initial_count, 0);

    bool removed = sine->remove_conditional_hook(condition);
    EXPECT_TRUE(removed);

    for (int i = 0; i < 20; i++) {
        sine->process_sample(0.0);
    }
    EXPECT_EQ(conditional_count, initial_count);
}

TEST_F(NodeCallbackTest, DuplicateCallbackPrevention)
{
    int callback_count = 0;

    auto callback = [&callback_count](const Nodes::NodeContext&) {
        callback_count++;
    };

    sine->on_tick(callback);
    sine->on_tick(callback);

    sine->process_sample(0.0);

    EXPECT_EQ(callback_count, 1);
}

TEST_F(NodeCallbackTest, ChainNodeCallbackPropagation)
{
    int source_count = 0;
    int target_count = 0;
    int chain_count = 0;

    auto source = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto target = std::make_shared<Nodes::Filters::FIR>(source, fir_coeffs);

    source->on_tick([&source_count](const Nodes::NodeContext&) {
        source_count++;
    });

    target->on_tick([&target_count](const Nodes::NodeContext&) {
        target_count++;
    });

    auto chain = source >> target;

    chain->on_tick([&chain_count](const Nodes::NodeContext&) {
        chain_count++;
    });

    MayaFlux::get_node_graph_manager()->add_to_root(chain);

    const int num_samples = 10;
    for (int i = 0; i < num_samples; i++) {
        MayaFlux::get_node_graph_manager()->get_root_node().process();
    }

    EXPECT_EQ(source_count, num_samples);
    EXPECT_EQ(target_count, num_samples);
    EXPECT_EQ(chain_count, num_samples);

    MayaFlux::get_node_graph_manager()->get_root_node().unregister_node(chain);

    chain->remove_all_hooks();
    source->remove_all_hooks();
    target->remove_all_hooks();

    // chain.reset();
    // target.reset();
    // source.reset();
}

TEST_F(NodeCallbackTest, NodeOperatorCallbacks)
{
    auto sine1 = std::make_shared<MayaFlux::Nodes::Generator::Sine>(440.0f, 0.5f);
    auto sine2 = std::make_shared<MayaFlux::Nodes::Generator::Sine>(880.0f, 0.3f);

    int sine1_count = 0;
    int sine2_count = 0;
    int add_node_count = 0;

    sine1->on_tick([&sine1_count](const Nodes::NodeContext&) {
        sine1_count++;
    });

    sine2->on_tick([&sine2_count](const Nodes::NodeContext&) {
        sine2_count++;
    });

    auto add_node = sine1 + sine2;

    add_node->on_tick([&add_node_count](const Nodes::NodeContext&) {
        add_node_count++;
    });

    const int num_samples = 10;
    for (int i = 0; i < num_samples; i++) {
        MayaFlux::get_node_graph_manager()->get_root_node().process();
    }

    EXPECT_EQ(sine1_count, num_samples);
    EXPECT_EQ(sine2_count, num_samples);
    EXPECT_EQ(add_node_count, num_samples);
}

TEST_F(NodeCallbackTest, ClearCallbacks)
{
    int callback_count = 0;

    auto sine1 = std::make_shared<MayaFlux::Nodes::Generator::Sine>(440.0f, 0.5f);

    sine1->on_tick([&callback_count](const MayaFlux::Nodes::NodeContext&) {
        callback_count++;
    });

    sine1->process_sample(0.0);
    EXPECT_EQ(callback_count, 1);

    sine1->remove_all_hooks();

    for (int i = 0; i < 10; i++) {
        sine1->process_sample(0.0);
    }

    EXPECT_EQ(callback_count, 1);
}

}
