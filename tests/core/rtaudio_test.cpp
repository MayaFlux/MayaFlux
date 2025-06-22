#include "../test_config.h"

#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"

#include "RtAudio.h"

// Define INTEGRATION_TEST to enable tests that interact with real audio devices
// Comment out for CI environments without audio hardware
// #define INTEGRATION_TEST

// Define AUDIBLE_TEST to run tests that produce audible output with longer durations
// This is useful for manual testing of audio functionality
// #define AUDIBLE_TEST

namespace MayaFlux::Test {

#ifdef INTEGRATION_TEST
void LogDeviceInfo(const RtAudio::DeviceInfo& info, const std::string& prefix = "")
{
    std::cout << prefix << "Name: " << info.name << std::endl;
    std::cout << prefix << "Output channels: " << info.outputChannels << std::endl;
    std::cout << prefix << "Input channels: " << info.inputChannels << std::endl;
    std::cout << prefix << "Duplex channels: " << info.duplexChannels << std::endl;
    std::cout << prefix << "Preferred sample rate: " << info.preferredSampleRate << std::endl;

    std::cout << prefix << "Supported sample rates: ";
    for (const auto& rate : info.sampleRates) {
        std::cout << rate << " ";
    }
    std::cout << std::endl;
}

class DeviceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        context = std::make_unique<RtAudio>();
        device = std::make_unique<MayaFlux::Core::Device>(context.get());
    }

    void TearDown() override
    {
        device.reset();
        context.reset();
    }

    std::unique_ptr<RtAudio> context;
    std::unique_ptr<MayaFlux::Core::Device> device;
};

TEST_F(DeviceTest, InitializationSucceeds)
{
    EXPECT_GT(device->get_output_devices().size(), 0)
        << "No output devices found - ensure audio system is available";
}

TEST_F(DeviceTest, DeviceEnumeration)
{
    const auto& outputDevices = device->get_output_devices();
    const auto& inputDevices = device->get_input_devices();

    for (const auto& deviceInfo : outputDevices) {
        EXPECT_GT(deviceInfo.outputChannels, 0);
        EXPECT_GT(deviceInfo.preferredSampleRate, 0);
        EXPECT_FALSE(deviceInfo.name.empty()) << "Device has empty name";
    }

    std::cout << "Found " << outputDevices.size() << " output devices and "
              << inputDevices.size() << " input devices" << std::endl;

    if (!outputDevices.empty()) {
        LogDeviceInfo(outputDevices[0], "Default output device: ");
    }

    unsigned int defaultDevice = device->get_default_output_device();
    if (defaultDevice > 0) {
        auto deviceInfo = context->getDeviceInfo(defaultDevice);
        EXPECT_GT(deviceInfo.outputChannels, 0);
    }
}

TEST_F(DeviceTest, SupportedSampleRates)
{
    unsigned int defaultDevice = device->get_default_output_device();
    auto deviceInfo = context->getDeviceInfo(defaultDevice);

    EXPECT_FALSE(deviceInfo.sampleRates.empty());

    std::cout << "Available sample rates for default device: ";
    for (const auto& rate : deviceInfo.sampleRates) {
        std::cout << rate << " ";
    }
    std::cout << std::endl;

    bool testRateSupported = std::find(deviceInfo.sampleRates.begin(),
                                 deviceInfo.sampleRates.end(),
                                 TestConfig::SAMPLE_RATE)
        != deviceInfo.sampleRates.end();

    if (!testRateSupported) {
        std::cout << "Note: Test sample rate " << TestConfig::SAMPLE_RATE
                  << " not directly supported by device. RtAudio will attempt resampling." << std::endl;
    }
}

class StreamTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = AudioTestHelper::createTestEngine();
    }

    void TearDown() override
    {
        if (engine) {
            engine->End();
        }
        engine.reset();
    }

    std::unique_ptr<Core::Engine> engine;
};

TEST_F(StreamTest, StreamCreationAndDestruction)
{
    const auto* stream = engine->get_stream_manager();
    ASSERT_NE(stream, nullptr) << "Stream manager not initialized";

    const auto& stream_info = engine->get_stream_info();
    EXPECT_EQ(stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(stream_info.num_channels, TestConfig::NUM_CHANNELS);

    EXPECT_FALSE(stream->is_running());
    EXPECT_FALSE(stream->is_open());

    EXPECT_NO_THROW({
        engine->Start();
        EXPECT_TRUE(stream->is_running());
        EXPECT_TRUE(stream->is_open());
    });

    EXPECT_NO_THROW({
        engine->End();
        EXPECT_FALSE(stream->is_running());
        EXPECT_FALSE(stream->is_open());
    });
}

TEST_F(StreamTest, StreamStateTransitions)
{
    const auto* stream = engine->get_stream_manager();

    EXPECT_FALSE(stream->is_running());
    EXPECT_FALSE(stream->is_open());

    engine->Start();
    EXPECT_TRUE(stream->is_running());
    EXPECT_TRUE(stream->is_open());

    engine->Pause();
    EXPECT_FALSE(stream->is_running());
    EXPECT_TRUE(stream->is_open());

    engine->Resume();
    EXPECT_TRUE(stream->is_running());
    EXPECT_TRUE(stream->is_open());

    engine->End();
    EXPECT_FALSE(stream->is_running());
    EXPECT_FALSE(stream->is_open());
}

TEST_F(StreamTest, ExcessiveChannelError)
{
    EXPECT_THROW({
        MayaFlux::Core::Engine badEngine;
        badEngine.Init({ .sample_rate = TestConfig::SAMPLE_RATE,
            .buffer_size = TestConfig::BUFFER_SIZE,
            .num_channels = 999 });
        badEngine.Start(); }, std::runtime_error);
}

TEST_F(StreamTest, BasicAudioProcessing)
{
    engine->Start();

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    engine->get_node_graph_manager()->add_to_root(sine);

    auto& root = engine->get_node_graph_manager()->get_root_node();
    std::cout << "Node count after registration: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    std::vector<double> output_buffer(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS, 0.0);
    engine->process_output(output_buffer.data(), TestConfig::BUFFER_SIZE);

    bool has_signal = false;
    for (const auto& sample : output_buffer) {
        if (std::abs(sample) > 0.01) {
            has_signal = true;
            break;
        }
    }

    EXPECT_TRUE(has_signal) << "No signal detected in output buffer";

    root.unregister_node(sine);
    engine->End();
}

#ifdef AUDIBLE_TEST
// Tests in this section are designed to produce audible output
// Run these tests manually when you want to hear the audio output

class AudibleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "=========================================" << std::endl;
        std::cout << "STARTING AUDIBLE TEST - YOU SHOULD HEAR AUDIO" << std::endl;
        std::cout << "=========================================" << std::endl;

        engine = AudioTestHelper::createTestEngine();

        engine->Start();
    }

    void TearDown() override
    {
        if (engine) {
            engine->End();
        }
        engine.reset();

        std::cout << "=========================================" << std::endl;
        std::cout << "AUDIBLE TEST COMPLETE" << std::endl;
        std::cout << "=========================================" << std::endl;
    }

    void waitForAudio(unsigned int ms = 2000)
    {
        std::cout << "Playing audio for " << ms << "ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    std::unique_ptr<Core::Engine> engine;
};

TEST_F(AudibleTest, AudioOutputTest)
{
    std::cout << "You should hear a 440Hz sine wave..." << std::endl;

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    engine->get_node_graph_manager()->add_to_root(sine);

    auto& root = engine->get_node_graph_manager()->get_root_node();
    std::cout << "Node count in graph: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    waitForAudio(3000);

    root.unregister_node(sine);
    std::cout << "Sine wave removed. Should now hear silence." << std::endl;

    waitForAudio(1000);
}

TEST_F(AudibleTest, FilteredAudioTest)
{
    std::cout << "You should hear a filtered sine wave..." << std::endl;

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    std::vector<double> coeffs(5, 0.2);
    auto filter = std::make_shared<Nodes::Filters::FIR>(sine, coeffs);

    engine->get_node_graph_manager()->add_to_root(filter);

    auto& root = engine->get_node_graph_manager()->get_root_node();
    std::cout << "Node count in graph: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    waitForAudio(3000);

    root.unregister_node(filter);
    waitForAudio(1000);
}

TEST_F(AudibleTest, StreamRestartTest)
{
    std::cout << "Testing stream restart with continuous audio..." << std::endl;

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    engine->get_node_graph_manager()->add_to_root(sine);

    auto& root = engine->get_node_graph_manager()->get_root_node();
    std::cout << "Node count before restart: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    waitForAudio(2000);

    std::cout << "Stopping audio..." << std::endl;
    engine->End();
    waitForAudio(1000);

    std::cout << "Restarting audio..." << std::endl;
    engine->Start();

    std::cout << "Node count after restart: " << root.get_node_size() << std::endl;

    if (root.get_node_size() == 0) {
        std::cout << "Re-adding node after restart..." << std::endl;
        engine->get_node_graph_manager()->add_to_root(sine);
        std::cout << "Updated node count: " << root.get_node_size() << std::endl;
    }

    waitForAudio(2000);

    root.unregister_node(sine);
    waitForAudio(1000);
}

TEST_F(AudibleTest, DiagnosticTest)
{
    std::cout << "Running audio diagnostic test..." << std::endl;

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.9f);

    std::cout << "About to add node to root" << std::endl;
    engine->get_node_graph_manager()->add_to_root(sine);

    auto& root = engine->get_node_graph_manager()->get_root_node();
    std::cout << "Node count in graph: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    std::cout << "Testing different frequencies..." << std::endl;

    std::vector<float> frequencies = { 440.0f, 880.0f, 220.0f };
    for (auto freq : frequencies) {
        std::cout << "Setting frequency to " << freq << "Hz" << std::endl;
        sine->set_frequency(freq);
        waitForAudio(1500);
    }

    std::cout << "Testing different amplitudes..." << std::endl;
    std::vector<float> amplitudes = { 0.2f, 0.5f, 0.9f };
    for (auto amp : amplitudes) {
        std::cout << "Setting amplitude to " << amp << std::endl;
        sine->set_amplitude(amp);
        waitForAudio(1500);
    }

    std::cout << "Final test - standard 440Hz tone for 5 seconds..." << std::endl;
    sine->set_frequency(440.0f);
    sine->set_amplitude(0.7f);
    waitForAudio(5000);

    root.unregister_node(sine);
}

TEST_F(AudibleTest, NoiseGeneratorTest)
{
    std::cout << "Testing noise generator..." << std::endl;

    auto noise = std::make_shared<Nodes::Generator::Stochastics::NoiseEngine>();
    noise->set_amplitude(0.3f);

    engine->get_node_graph_manager()->add_to_root(noise);

    auto& root = engine->get_node_graph_manager()->get_root_node();
    std::cout << "Node count in graph: " << root.get_node_size() << std::endl;
    EXPECT_EQ(root.get_node_size(), 1);

    struct NoiseTest {
        Utils::distribution type;
        std::string name;
    };

    std::vector<NoiseTest> noiseTypes = {
        { Utils::distribution::UNIFORM, "Uniform" },
        { Utils::distribution::NORMAL, "Normal (Gaussian)" },
        { Utils::distribution::EXPONENTIAL, "Exponential" },
        { Utils::distribution::POISSON, "Poisson" }
    };

    for (const auto& test : noiseTypes) {
        std::cout << "Testing " << test.name << " noise..." << std::endl;
        noise->set_type(test.type);
        waitForAudio(2000);
    }

    root.unregister_node(noise);
}
#endif // AUDIBLE_TEST

#endif // INTEGRATION_TEST

class RtAudioUtilityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        engine = MayaFlux::Test::AudioTestHelper::createTestEngine();
    }

    void TearDown() override
    {
        engine.reset();
    }

    std::unique_ptr<MayaFlux::Core::Engine> engine;
};

TEST_F(RtAudioUtilityTest, StreamInfoVerification)
{
    const auto& info = engine->get_stream_info();

    EXPECT_EQ(info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(RtAudioUtilityTest, GlobalInitialization)
{
    MayaFlux::End();

    MayaFlux::Init(44100, 256, 1);

    EXPECT_TRUE(MayaFlux::is_engine_initialized());
    EXPECT_EQ(MayaFlux::get_sample_rate(), 44100);
    EXPECT_EQ(MayaFlux::get_buffer_size(), 256);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), 1);

    MayaFlux::End();
}

TEST_F(RtAudioUtilityTest, DeviceInfoAccess)
{
    RtAudio direct_context;

    unsigned int deviceCount = direct_context.getDeviceCount();
    std::cout << "RtAudio reports " << deviceCount << " devices" << std::endl;

    RtAudio::Api api = direct_context.getCurrentApi();
    std::cout << "Using audio API: " << api << std::endl;
    EXPECT_NE(api, RtAudio::Api::UNSPECIFIED);

    std::string version = direct_context.getVersion();
    EXPECT_FALSE(version.empty());
    std::cout << "RtAudio version: " << version << std::endl;
}

} // namespace MayaFlux::Test
