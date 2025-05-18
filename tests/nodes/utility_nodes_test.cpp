#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Impulse.hpp"
#include "MayaFlux/Nodes/Generators/Phasor.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {

class ImpulseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        impulse = std::make_shared<Nodes::Generator::Impulse>(0.5);
    }

    std::shared_ptr<Nodes::Generator::Impulse> impulse;
};

TEST_F(ImpulseTest, BasicProperties)
{
    EXPECT_FLOAT_EQ(impulse->get_amplitude(), 1.0);
    EXPECT_FLOAT_EQ(impulse->get_frequency(), 0.5);

    impulse->set_amplitude(0.8);
    EXPECT_FLOAT_EQ(impulse->get_amplitude(), 0.8);

    impulse->set_frequency(2.0);
    EXPECT_FLOAT_EQ(impulse->get_frequency(), 2.0);
}

TEST_F(ImpulseTest, SingleImpulse)
{
    double result = impulse->process_sample(0.0);
    EXPECT_DOUBLE_EQ(result, impulse->get_amplitude());

    for (int i = 0; i < 10; i++) {
        result = impulse->process_sample(0.0);
        EXPECT_DOUBLE_EQ(result, 0.0);
    }
}

TEST_F(ImpulseTest, ImpulseCallback)
{
    int impulse_callback_count = 0;

    impulse->on_impulse([&impulse_callback_count](const Nodes::NodeContext& ctx) {
        impulse_callback_count++;
    });

    impulse->process_sample(0.0);
    EXPECT_EQ(impulse_callback_count, 1);

    for (int i = 0; i < 10; i++) {
        impulse->process_sample(0.0);
    }
    EXPECT_EQ(impulse_callback_count, 1);

    impulse->reset();
    impulse->process_sample(0.0);
    EXPECT_EQ(impulse_callback_count, 2);
}

TEST_F(ImpulseTest, ConditionalCallback)
{
    int conditional_callback_count = 0;

    impulse->on_tick_if(
        [&conditional_callback_count](const Nodes::NodeContext& ctx) {
            conditional_callback_count++;
        },
        [](const Nodes::NodeContext& ctx) {
            return ctx.value > 0.5;
        });

    impulse->set_amplitude(1.0);

    impulse->process_sample(0.0);
    EXPECT_EQ(conditional_callback_count, 1);

    impulse->set_amplitude(0.4);
    impulse->reset();
    impulse->set_frequency(880);
    impulse->process_batch(1760);
    EXPECT_EQ(conditional_callback_count, 34);
}

TEST_F(ImpulseTest, ProcessBatch)
{
    unsigned int buffer_size = 10;
    std::vector<double> buffer = impulse->process_batch(buffer_size);

    EXPECT_EQ(buffer.size(), buffer_size);
    EXPECT_DOUBLE_EQ(buffer[0], impulse->get_amplitude());

    for (size_t i = 1; i < buffer_size; i++) {
        EXPECT_DOUBLE_EQ(buffer[i], 0.0);
    }
}

class PhasorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        phasor = std::make_shared<Nodes::Generator::Phasor>(1.0);
    }

    std::shared_ptr<Nodes::Generator::Phasor> phasor;
};

TEST_F(PhasorTest, BasicProperties)
{
    EXPECT_FLOAT_EQ(phasor->get_frequency(), 1.0);
    EXPECT_DOUBLE_EQ(phasor->get_phase(), 0.0);

    phasor->set_frequency(2.0);
    EXPECT_FLOAT_EQ(phasor->get_frequency(), 2.0);

    phasor->reset(1.0, 1.0, 0.0, 0.25);
    EXPECT_DOUBLE_EQ(phasor->get_phase(), 0.25);
}

TEST_F(PhasorTest, PhaseProgression)
{
    double expected_phase_increment = phasor->get_frequency() / TestConfig::SAMPLE_RATE;
    double phase = 0.0;

    for (int i = 0; i < 10; i++) {
        double result = phasor->process_sample(0.0);

        EXPECT_NEAR(result, phase * phasor->get_amplitude(), 1e-6);

        phase += expected_phase_increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}

TEST_F(PhasorTest, PhaseWrapCallback)
{
    int wrap_callback_count = 0;

    phasor->on_phase_wrap([&wrap_callback_count](const Nodes::NodeContext& ctx) {
        wrap_callback_count++;
    });

    phasor->set_frequency(TestConfig::SAMPLE_RATE / 4.0); // Wraps around every 4 samples

    for (int i = 0; i < 10; i++) {
        phasor->process_sample(0.0);
    }

    EXPECT_GT(wrap_callback_count, 0);
}

TEST_F(PhasorTest, ThresholdCallback)
{
    int threshold_callback_count = 0;
    double threshold = 0.5;

    phasor->on_threshold(
        [&threshold_callback_count](const Nodes::NodeContext& ctx) {
            threshold_callback_count++;
        },
        threshold);

    for (int i = 0; i < TestConfig::SAMPLE_RATE; i++) {
        phasor->process_sample(0.0);
    }

    EXPECT_EQ(threshold_callback_count, 1);
}

TEST_F(PhasorTest, FrequencyChange)
{
    for (int i = 0; i < 5; i++) {
        phasor->process_sample(0.0);
    }

    phasor->set_frequency(2.0);

    double expected_phase_increment = 2.0 / TestConfig::SAMPLE_RATE;
    double last_value = phasor->process_sample(0.0);

    for (int i = 0; i < 5; i++) {
        double result = phasor->process_sample(0.0);
        double actual_increment = result - last_value;
        if (actual_increment < 0)
            actual_increment += 1.0; // Handle wraparound
        EXPECT_NEAR(actual_increment, expected_phase_increment, 1e-6);
        last_value = result;
    }
}

TEST_F(PhasorTest, FrequencyModulation)
{
    auto freq_mod = std::make_shared<Nodes::Generator::Sine>(5.0, 0.5);

    auto modulated_phasor = std::make_shared<Nodes::Generator::Phasor>(freq_mod, 1.0);

    std::vector<double> modulated = modulated_phasor->process_batch(20);
    std::vector<double> unmodulated = phasor->process_batch(20);

    bool differences_found = false;
    for (size_t i = 0; i < modulated.size(); i++) {
        if (std::abs(modulated[i] - unmodulated[i]) > 1e-6) {
            differences_found = true;
            break;
        }
    }

    EXPECT_TRUE(differences_found);
}

TEST_F(PhasorTest, AmplitudeModulation)
{
    auto amp_mod = std::make_shared<Nodes::Generator::Sine>(5.0, 0.5);

    auto modulated_phasor = std::make_shared<Nodes::Generator::Phasor>(1.0, amp_mod);

    std::vector<double> modulated = modulated_phasor->process_batch(20);
    std::vector<double> unmodulated = phasor->process_batch(20);

    bool differences_found = false;
    for (size_t i = 0; i < modulated.size(); i++) {
        if (std::abs(modulated[i] - unmodulated[i]) > 1e-6) {
            differences_found = true;
            break;
        }
    }

    EXPECT_TRUE(differences_found);
}

TEST_F(PhasorTest, RemoveCallbacks)
{
    int callback_count = 0;

    auto callback = [&callback_count](const Nodes::NodeContext& ctx) {
        callback_count++;
    };

    phasor->on_tick(callback);

    phasor->process_sample(0.0);
    EXPECT_EQ(callback_count, 1);

    bool removed = phasor->remove_hook(callback);
    EXPECT_TRUE(removed);

    phasor->process_sample(0.0);
    EXPECT_EQ(callback_count, 1);
}

/* TEST_F(PhasorTest, NodeContext)
{
    double test_value = 0.5;
    auto context = phasor->create_context(test_value);

    // Check that context contains expected values
    EXPECT_DOUBLE_EQ(context->value, test_value);

    // Cast to GeneratorContext to access generator-specific fields
    auto gen_context = dynamic_cast<Nodes::Generator::GeneratorContext*>(context.get());
    ASSERT_NE(gen_context, nullptr);

    EXPECT_FLOAT_EQ(gen_context->frequency, phasor->get_frequency());
    EXPECT_FLOAT_EQ(gen_context->amplitude, phasor->get_amplitude());
    EXPECT_DOUBLE_EQ(gen_context->phase, phasor->get_phase());
} */

} // namespace MayaFlux::Test
